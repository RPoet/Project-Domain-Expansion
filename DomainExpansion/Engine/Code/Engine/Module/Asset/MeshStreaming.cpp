#include "Engine/Module/Asset/MeshStreaming.h"

#include "Engine/Framework/FrameworkFileSystem.h"
#include "Render/Backends/RenderBackend.h"

static uint32 getDefaultRequiredVertexBufferFlags()
{
	return getMeshBufferSignatureFlag(MeshBufferSignature::position)
		| getMeshBufferSignatureFlag(MeshBufferSignature::normal)
		| getMeshBufferSignatureFlag(MeshBufferSignature::texcoord);
}

static void initializeMeshAssetHandleVertexBuffers(MeshAssetHandle& handle)
{
	if (handle.requiredVertexBufferFlags == 0)
	{
		handle.requiredVertexBufferFlags = getDefaultRequiredVertexBufferFlags();
	}

	handle.activeVertexBufferFlags = 0;
	for (uint32 signatureIndex = 0; signatureIndex < meshVertexBufferSignatureCount; ++signatureIndex)
	{
		handle.vertexBufferObjects[signatureIndex].reset();
		handle.vertexBufferSizesInBytes[signatureIndex] = 0;
		handle.vertexBufferStridesInBytes[signatureIndex] = 0;
	}

	handle.vertexBufferStridesInBytes[getMeshBufferSignatureIndex(MeshBufferSignature::position)] =
		static_cast<uint32>(sizeof(MeshAsset::PositionData));
	handle.vertexBufferStridesInBytes[getMeshBufferSignatureIndex(MeshBufferSignature::normal)] =
		static_cast<uint32>(sizeof(MeshAsset::NormalData));
	handle.vertexBufferStridesInBytes[getMeshBufferSignatureIndex(MeshBufferSignature::texcoord)] =
		static_cast<uint32>(sizeof(MeshAsset::TexcoordData));

	handle.indexBufferObject.reset();
	handle.indexBufferSizeInBytes = 0;
}

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
	initializeMeshAssetHandleVertexBuffers(*handle);
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
			   << " positionBufferBytes=" << handle->getBufferSizeInBytes(MeshBufferSignature::position)
			   << " normalBufferBytes=" << handle->getBufferSizeInBytes(MeshBufferSignature::normal)
			   << " texcoordBufferBytes=" << handle->getBufferSizeInBytes(MeshBufferSignature::texcoord)
			   << " indexBufferBytes=" << handle->indexBufferSizeInBytes
			   << " activeVertexBufferFlags=" << handle->activeVertexBufferFlags << lineBreak;
	}
}

bool MeshStreaming::uploadMeshHandleToGpu(
	RenderBackend& renderBackend,
	MeshAssetHandle& handle,
	string& outErrorText) const
{
	outErrorText.clear();
	initializeMeshAssetHandleVertexBuffers(handle);

	if (handle.meshAsset == nullptr)
	{
		outErrorText = "mesh_asset_missing";
		return false;
	}

	const MeshAsset& meshAsset = *handle.meshAsset;
	const uint64 indexBufferBytes = static_cast<uint64>(meshAsset.indices.size()) * sizeof(uint32);
	uint64 vertexBufferSizesInBytes[meshVertexBufferSignatureCount] = {};
	const void* vertexBufferInitialData[meshVertexBufferSignatureCount] = {};
	uint32 vertexBufferElementCounts[meshVertexBufferSignatureCount] = {};
	const char* vertexBufferCreateFailReasons[meshVertexBufferSignatureCount] = {};

	uint32 sourceVertexBufferFlags = 0;
	for (uint32 signatureIndex = 0; signatureIndex < meshVertexBufferSignatureCount; ++signatureIndex)
	{
		const MeshBufferSignature signature = static_cast<MeshBufferSignature>(signatureIndex);
		const uint32 signatureFlag = getMeshBufferSignatureFlag(signature);
		uint64 bufferByteSize = 0;
		const void* initialData = nullptr;
		uint32 elementCount = 0;
		const char* createFailReason = "buffer_create_failed";

		switch (signature)
		{
		case MeshBufferSignature::position:
			bufferByteSize = static_cast<uint64>(meshAsset.positionVertices.size()) * sizeof(MeshAsset::PositionData);
			initialData = meshAsset.positionVertices.data();
			elementCount = static_cast<uint32>(meshAsset.positionVertices.size());
			createFailReason = "position_buffer_create_failed";
			break;
		case MeshBufferSignature::normal:
			bufferByteSize = static_cast<uint64>(meshAsset.normalVertices.size()) * sizeof(MeshAsset::NormalData);
			initialData = meshAsset.normalVertices.data();
			elementCount = static_cast<uint32>(meshAsset.normalVertices.size());
			createFailReason = "normal_buffer_create_failed";
			break;
		case MeshBufferSignature::texcoord:
			bufferByteSize = static_cast<uint64>(meshAsset.texcoordVertices.size()) * sizeof(MeshAsset::TexcoordData);
			initialData = meshAsset.texcoordVertices.data();
			elementCount = static_cast<uint32>(meshAsset.texcoordVertices.size());
			createFailReason = "texcoord_buffer_create_failed";
			break;
		case MeshBufferSignature::count:
		default:
			outErrorText = "mesh_buffer_signature_invalid";
			return false;
		}

		vertexBufferSizesInBytes[signatureIndex] = bufferByteSize;
		vertexBufferInitialData[signatureIndex] = initialData;
		vertexBufferElementCounts[signatureIndex] = elementCount;
		vertexBufferCreateFailReasons[signatureIndex] = createFailReason;
		if (bufferByteSize > 0)
		{
			sourceVertexBufferFlags |= signatureFlag;
		}

		if ((handle.requiredVertexBufferFlags & signatureFlag) == 0)
		{
			continue;
		}

		if (bufferByteSize == 0)
		{
			outErrorText = "mesh_data_empty";
			return false;
		}

		if (bufferByteSize > static_cast<uint64>(uint32MaxValue))
		{
			outErrorText = "mesh_buffer_size_overflow";
			return false;
		}

		if (vertexBufferElementCounts[signatureIndex] != meshAsset.vertexCount)
		{
			outErrorText = "mesh_vertex_stream_mismatch";
			return false;
		}
	}

	if ((sourceVertexBufferFlags & handle.requiredVertexBufferFlags) != handle.requiredVertexBufferFlags)
	{
		outErrorText = "mesh_required_vertex_buffer_missing";
		return false;
	}

	if (indexBufferBytes == 0)
	{
		outErrorText = "mesh_data_empty";
		return false;
	}

	if (indexBufferBytes > static_cast<uint64>(uint32MaxValue))
	{
		outErrorText = "mesh_buffer_size_overflow";
		return false;
	}

	for (uint32 signatureIndex = 0; signatureIndex < meshVertexBufferSignatureCount; ++signatureIndex)
	{
		const MeshBufferSignature signature = static_cast<MeshBufferSignature>(signatureIndex);
		const uint32 signatureFlag = getMeshBufferSignatureFlag(signature);
		if ((handle.requiredVertexBufferFlags & signatureFlag) == 0)
		{
			continue;
		}

		BufferObjectCreateOptions bufferCreateOptions = {};
		bufferCreateOptions.sizeInBytes = vertexBufferSizesInBytes[signatureIndex];
		bufferCreateOptions.initialData = vertexBufferInitialData[signatureIndex];
		if (bufferCreateOptions.sizeInBytes == 0 || bufferCreateOptions.initialData == nullptr)
		{
			outErrorText = "mesh_data_empty";
			return false;
		}

		unique_pointer<BufferResourceObject> createdBufferObject =
			renderBackend.createBufferObject(bufferCreateOptions);
		if (createdBufferObject == nullptr)
		{
			outErrorText = vertexBufferCreateFailReasons[signatureIndex];
			return false;
		}

		handle.vertexBufferObjects[signatureIndex] = moveValue(createdBufferObject);
		handle.vertexBufferSizesInBytes[signatureIndex] = static_cast<uint32>(bufferCreateOptions.sizeInBytes);
		handle.activeVertexBufferFlags |= signatureFlag;
	}

	if ((handle.activeVertexBufferFlags & handle.requiredVertexBufferFlags) != handle.requiredVertexBufferFlags)
	{
		outErrorText = "mesh_vertex_upload_flag_mismatch";
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

	handle.indexBufferObject = moveValue(indexBufferObject);
	handle.indexBufferSizeInBytes = static_cast<uint32>(indexBufferBytes);

	if (handle.indexBufferObject == nullptr || handle.indexBufferSizeInBytes == 0)
	{
		outErrorText = "mesh_index_upload_invalid";
		return false;
	}

	return true;
}
