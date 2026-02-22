#include "Engine/Module/Asset/MeshStreaming.h"

#include "Engine/Framework/FrameworkFileSystem.h"
#include "Render/Backends/RenderBackend.h"

bool MeshStreaming::init(Framework& framework)
{
	unused(framework);
	clear();
	return true;
}

void MeshStreaming::preUpdate()
{
}

void MeshStreaming::postUpdate()
{
	flushCpuRequests();
}

shared_pointer<MeshAssetHandle> MeshStreaming::requestMesh(
	const string& meshRelativePath,
	const uint32 lodLevel)
{
	const string cacheKey = getMeshCacheKey(meshRelativePath, lodLevel);
	auto foundHandle = handleCache.find(cacheKey);
	if (foundHandle != handleCache.end())
	{
		return foundHandle->second;
	}

	shared_pointer<MeshAssetHandle> handle(new MeshAssetHandle());
	handle->meshRelativePath = meshRelativePath;
	handle->lodLevel = lodLevel;
	handle->state = MeshAssetHandleState::pending;
	handleCache.emplace(cacheKey, handle);
	pendingHandles.push_back(handle);
	return handle;
}

void MeshStreaming::shutdown()
{
	clear();
}

void MeshStreaming::flushCpuRequests()
{
	if (!pendingHandles.empty())
	{
		vector<shared_pointer<MeshAssetHandle>> processingHandles;
		processingHandles.swap(pendingHandles);

		for (uint32 handleIndex = 0; handleIndex < static_cast<uint32>(processingHandles.size()); ++handleIndex)
		{
			shared_pointer<MeshAssetHandle>& handle = processingHandles[handleIndex];
			if (handle == nullptr || handle->state != MeshAssetHandleState::pending)
			{
				continue;
			}

			string resolvedMeshPath = {};
			if (!resolveMeshAbsolutePath(handle->meshRelativePath, resolvedMeshPath))
			{
				handle->state = MeshAssetHandleState::failed;
				const char* const failReason = "mesh_path_resolve_failed";
				error << "[MeshStreaming][Error] mesh=" << handle->meshRelativePath
					  << " lod=" << handle->lodLevel
					  << " reason=" << failReason << lineBreak;
				continue;
			}

			MeshAsset meshAsset = {};
			string errorText = {};
			if (!MeshParser::get().parseFromFile(resolvedMeshPath, handle->lodLevel, meshAsset, errorText))
			{
				handle->state = MeshAssetHandleState::failed;
				const string failReason = errorText.empty() ? "mesh_load_failed" : errorText;
				error << "[MeshStreaming][Error] mesh=" << handle->meshRelativePath
					  << " lod=" << handle->lodLevel
					  << " reason=" << failReason << lineBreak;
				continue;
			}

			handle->meshAsset = shared_pointer<MeshAsset>(new MeshAsset(moveValue(meshAsset)));
			handle->state = MeshAssetHandleState::ready;
			handle->gpuState = MeshAssetGpuState::pending;
			pendingGpuUploadHandles.push_back(handle);
			output << "[MeshStreaming][Ready] mesh=" << handle->meshRelativePath
				   << " lod=" << handle->lodLevel
				   << " vertexCount=" << handle->meshAsset->vertexCount
				   << " indexCount=" << handle->meshAsset->indexCount << lineBreak;
		}
	}
}

void MeshStreaming::clear()
{
	handleCache.clear();
	pendingHandles.clear();
	pendingGpuUploadHandles.clear();
}

uint32 MeshStreaming::getPendingRequestCount() const
{
	return static_cast<uint32>(pendingHandles.size() + pendingGpuUploadHandles.size());
}

string MeshStreaming::getMeshCacheKey(
	const string& meshRelativePath,
	const uint32 lodLevel) const
{
	return meshRelativePath + "|LOD" + std::to_string(lodLevel);
}

bool MeshStreaming::resolveMeshAbsolutePath(
	const string& meshRelativePath,
	string& outAbsolutePath) const
{
	outAbsolutePath.clear();
	return frameworkFileSystemResolvePathFromResources(meshRelativePath, outAbsolutePath);
}

void MeshStreaming::flushGpuRequests(RenderBackend& renderBackend)
{
	if (pendingGpuUploadHandles.empty())
	{
		return;
	}

	vector<shared_pointer<MeshAssetHandle>> processingHandles;
	processingHandles.swap(pendingGpuUploadHandles);
	for (uint32 handleIndex = 0; handleIndex < static_cast<uint32>(processingHandles.size()); ++handleIndex)
	{
		shared_pointer<MeshAssetHandle>& handle = processingHandles[handleIndex];
		if (handle == nullptr
			|| handle->state != MeshAssetHandleState::ready
			|| handle->gpuState != MeshAssetGpuState::pending)
		{
			continue;
		}

		string errorText = {};
		if (!uploadMeshHandleToGpu(renderBackend, *handle, errorText))
		{
			handle->gpuState = MeshAssetGpuState::failed;
			const string failReason = errorText.empty() ? "gpu_upload_failed" : errorText;
			error << "[MeshStreaming][Error] mesh=" << handle->meshRelativePath
				  << " lod=" << handle->lodLevel
				  << " reason=" << failReason << lineBreak;
			continue;
		}

		handle->gpuState = MeshAssetGpuState::ready;
		output << "[MeshStreaming][GpuReady] mesh=" << handle->meshRelativePath
			   << " lod=" << handle->lodLevel
			   << " vertexBufferBytes=" << handle->vertexBufferSizeInBytes
			   << " indexBufferBytes=" << handle->indexBufferSizeInBytes << lineBreak;
	}
}

bool MeshStreaming::uploadMeshHandleToGpu(
	RenderBackend& renderBackend,
	MeshAssetHandle& handle,
	string& outErrorText) const
{
	outErrorText.clear();
	handle.vertexBufferObject.reset();
	handle.indexBufferObject.reset();
	handle.vertexBufferSizeInBytes = 0;
	handle.indexBufferSizeInBytes = 0;

	if (handle.meshAsset == nullptr)
	{
		outErrorText = "mesh_asset_missing";
		return false;
	}

	const MeshAsset& meshAsset = *handle.meshAsset;
	const uint64 vertexBufferBytes = static_cast<uint64>(meshAsset.vertices.size()) * sizeof(MeshAsset::VertexData);
	const uint64 indexBufferBytes = static_cast<uint64>(meshAsset.indices.size()) * sizeof(uint32);
	if (vertexBufferBytes == 0 || indexBufferBytes == 0)
	{
		outErrorText = "mesh_data_empty";
		return false;
	}

	if (vertexBufferBytes > static_cast<uint64>(uint32MaxValue)
		|| indexBufferBytes > static_cast<uint64>(uint32MaxValue))
	{
		outErrorText = "mesh_buffer_size_overflow";
		return false;
	}

	BufferObjectCreateOptions vertexBufferCreateOptions = {};
	vertexBufferCreateOptions.sizeInBytes = vertexBufferBytes;
	vertexBufferCreateOptions.initialData = meshAsset.vertices.data();
	unique_pointer<BufferResourceObject> vertexBufferObject = renderBackend.createBufferObject(vertexBufferCreateOptions);
	if (vertexBufferObject == nullptr)
	{
		outErrorText = "vertex_buffer_create_failed";
		return false;
	}

	BufferObjectCreateOptions indexBufferCreateOptions = {};
	indexBufferCreateOptions.sizeInBytes = indexBufferBytes;
	indexBufferCreateOptions.initialData = meshAsset.indices.data();
	unique_pointer<BufferResourceObject> indexBufferObject = renderBackend.createBufferObject(indexBufferCreateOptions);
	if (indexBufferObject == nullptr)
	{
		outErrorText = "index_buffer_create_failed";
		return false;
	}

	handle.vertexBufferObject = moveValue(vertexBufferObject);
	handle.indexBufferObject = moveValue(indexBufferObject);
	handle.vertexBufferSizeInBytes = static_cast<uint32>(vertexBufferBytes);
	handle.indexBufferSizeInBytes = static_cast<uint32>(indexBufferBytes);
	return true;
}
