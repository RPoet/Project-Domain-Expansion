#include "Engine/Module/Asset/MeshStreaming.h"

#include "Engine/Framework/FrameworkFileSystem.h"
#include "Engine/Module/Render/GPUUploader.h"
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
	shared_pointer<GPUUploader> gpuUploader = GPUUploader::get();
	if (gpuUploader == nullptr)
	{
		error << "[MeshStreaming][Error] reason=gpu_uploader_missing" << lineBreak;
		return;
	}

	SyncObject* syncObject = renderBackend.getSyncObject();
	if (syncObject != nullptr)
	{
		gpuUploader->setFenceValue(syncObject->getCompletedFenceValue());
	}

	if (pendingGpuUploadHandles.empty() && !gpuUploader->hasQueuedUploadRequests())
	{
		return;
	}

	vector<shared_pointer<MeshAssetHandle>> processingHandles;
	processingHandles.swap(pendingGpuUploadHandles);

	CommandList* uploadCommandList = renderBackend.acquireCommandList();
	if (uploadCommandList == nullptr)
	{
		error << "[MeshStreaming][Error] reason=gpu_upload_command_list_acquire_failed" << lineBreak;
		for (uint32 handleIndex = 0; handleIndex < static_cast<uint32>(processingHandles.size()); ++handleIndex)
		{
			shared_pointer<MeshAssetHandle>& handle = processingHandles[handleIndex];
			if (handle == nullptr || handle->gpuState != MeshAssetGpuState::pending)
			{
				continue;
			}

			handle->gpuState = MeshAssetGpuState::failed;
			error << "[MeshStreaming][Error] mesh=" << handle->meshRelativePath
				  << " lod=" << handle->lodLevel
				  << " reason=gpu_upload_command_list_acquire_failed" << lineBreak;
		}
		return;
	}

	vector<shared_pointer<MeshAssetHandle>> uploadedHandles;
	for (uint32 handleIndex = 0; handleIndex < static_cast<uint32>(processingHandles.size()); ++handleIndex)
	{
		shared_pointer<MeshAssetHandle>& handle = processingHandles[handleIndex];
		if (handle == nullptr
			|| handle->state != MeshAssetHandleState::ready
			|| handle->gpuState != MeshAssetGpuState::pending)
		{
			continue;
		}

		if (!uploadMeshHandleToGpu(renderBackend, *handle))
		{
			handle->gpuState = MeshAssetGpuState::failed;
			continue;
		}

		uploadedHandles.push_back(handle);
	}

	if (gpuUploader->hasQueuedUploadRequests())
	{
		uploadCommandList->reset();
		gpuUploader->uploadQueuedBuffers(*uploadCommandList);
		uploadCommandList->close();
		renderBackend.queueCommandList(uploadCommandList);
	}
	else
	{
		renderBackend.releaseCommandList(uploadCommandList);
	}

	for (uint32 handleIndex = 0; handleIndex < static_cast<uint32>(uploadedHandles.size()); ++handleIndex)
	{
		shared_pointer<MeshAssetHandle>& handle = uploadedHandles[handleIndex];
		if (handle == nullptr)
		{
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
	MeshAssetHandle& handle) const
{
	initializeMeshAssetHandleVertexBuffers(handle);
	const auto failUpload = [&handle](const char* reason) -> bool
	{
		error << "[MeshStreaming][Error] mesh=" << handle.meshRelativePath
			  << " lod=" << handle.lodLevel
			  << " reason=" << reason << lineBreak;
		return false;
	};

	shared_pointer<GPUUploader> gpuUploader = GPUUploader::get();
	if (gpuUploader == nullptr)
	{
		return failUpload("gpu_uploader_missing");
	}

	if (handle.meshAsset == nullptr)
	{
		return failUpload("mesh_asset_missing");
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
			return failUpload("mesh_buffer_signature_invalid");
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
			return failUpload("mesh_data_empty");
		}

		if (bufferByteSize > static_cast<uint64>(uint32MaxValue))
		{
			return failUpload("mesh_buffer_size_overflow");
		}

		if (vertexBufferElementCounts[signatureIndex] != meshAsset.vertexCount)
		{
			return failUpload("mesh_vertex_stream_mismatch");
		}
	}

	if ((sourceVertexBufferFlags & handle.requiredVertexBufferFlags) != handle.requiredVertexBufferFlags)
	{
		return failUpload("mesh_required_vertex_buffer_missing");
	}

	if (indexBufferBytes == 0)
	{
		return failUpload("mesh_data_empty");
	}

	if (indexBufferBytes > static_cast<uint64>(uint32MaxValue))
	{
		return failUpload("mesh_buffer_size_overflow");
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

		BufferUploadRequestOptions uploadRequestOptions = {};
		uploadRequestOptions.sourceData = vertexBufferInitialData[signatureIndex];
		uploadRequestOptions.sourceDataSizeInBytes = vertexBufferSizesInBytes[signatureIndex];
		uploadRequestOptions.destinationOffsetInBytes = 0;
		if (bufferCreateOptions.sizeInBytes == 0 || uploadRequestOptions.sourceData == nullptr)
		{
			return failUpload("mesh_data_empty");
		}

		unique_pointer<BufferResourceObject> createdBufferObject = gpuUploader->createBufferObject(renderBackend, bufferCreateOptions, uploadRequestOptions);
		if (createdBufferObject == nullptr)
		{
			return failUpload(vertexBufferCreateFailReasons[signatureIndex]);
		}

		handle.vertexBufferObjects[signatureIndex] = moveValue(createdBufferObject);
		handle.vertexBufferSizesInBytes[signatureIndex] = static_cast<uint32>(bufferCreateOptions.sizeInBytes);
		handle.activeVertexBufferFlags |= signatureFlag;
	}

	if ((handle.activeVertexBufferFlags & handle.requiredVertexBufferFlags) != handle.requiredVertexBufferFlags)
	{
		return failUpload("mesh_vertex_upload_flag_mismatch");
	}

	BufferObjectCreateOptions indexBufferCreateOptions = {};
	indexBufferCreateOptions.sizeInBytes = indexBufferBytes;

	BufferUploadRequestOptions indexUploadRequestOptions = {};
	indexUploadRequestOptions.sourceData = meshAsset.indices.data();
	indexUploadRequestOptions.sourceDataSizeInBytes = indexBufferBytes;
	indexUploadRequestOptions.destinationOffsetInBytes = 0;
	unique_pointer<BufferResourceObject> indexBufferObject = gpuUploader->createBufferObject(renderBackend, indexBufferCreateOptions, indexUploadRequestOptions);
	if (indexBufferObject == nullptr)
	{
		return failUpload("index_buffer_create_failed");
	}

	handle.indexBufferObject = moveValue(indexBufferObject);
	handle.indexBufferSizeInBytes = static_cast<uint32>(indexBufferBytes);

	if (handle.indexBufferObject == nullptr || handle.indexBufferSizeInBytes == 0)
	{
		return failUpload("mesh_index_upload_invalid");
	}

	return true;
}
