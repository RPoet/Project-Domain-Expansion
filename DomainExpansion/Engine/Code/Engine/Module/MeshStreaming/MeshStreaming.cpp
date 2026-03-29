#include "Engine/Module/MeshStreaming/MeshStreaming.h"

#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/Render/GPUUploader.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Render/Backends/RenderBackend.h"

[[noreturn]] static void failUpload(const char* reason)
{
	unused(reason);
	assert(false && "[MeshStreaming][Assert] reason=mesh_gpu_upload_failed");
}

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
		static_cast<uint32>(sizeof(PositionData));
	handle.vertexBufferStridesInBytes[getMeshBufferSignatureIndex(MeshBufferSignature::normal)] =
		static_cast<uint32>(sizeof(NormalData));
	handle.vertexBufferStridesInBytes[getMeshBufferSignatureIndex(MeshBufferSignature::texcoord)] =
		static_cast<uint32>(sizeof(TexcoordData));

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
	const shared_pointer<MeshAsset>& meshAsset,
	const uint32 lodLevel)
{
	assert(meshAsset != nullptr && "[MeshStreaming][Assert] reason=mesh_asset_missing");
	const string& meshAssetPath = meshAsset->getAssetPath();
	assert(!meshAssetPath.empty() && "[MeshStreaming][Assert] reason=mesh_asset_path_missing");
	assert(lodLevel < meshAsset->getLODCount() && "[MeshStreaming][Assert] reason=mesh_asset_lod_out_of_range");

	const string cacheKey = getMeshCacheKey(meshAssetPath, lodLevel);
	auto foundHandle = handleCache.find(cacheKey);
	if (foundHandle != handleCache.end())
	{
		return foundHandle->second;
	}

	shared_pointer<MeshAssetHandle> handle(new MeshAssetHandle());
	handle->meshAssetPath = meshAssetPath;
	handle->meshAsset = meshAsset;
	handle->lodLevel = lodLevel;
	handle->state = MeshAssetHandleState::ready;
	handle->gpuState = MeshAssetGpuState::pending;
	initializeMeshAssetHandleVertexBuffers(*handle);
	handleCache.emplace(cacheKey, handle);
	pendingGpuUploadHandles.push_back(handle);
	return handle;
}

void MeshStreaming::shutdown()
{
	clear();
}

void MeshStreaming::flushCpuRequests()
{
}

void MeshStreaming::clear()
{
	shared_pointer<GPUUploader> gpuUploader = GPUUploader::get();
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	if (gpuUploader != nullptr && !inFlightUploadSubmissions.empty())
	{
		gpuUploader->waitForUploadCompletion();
		gpuUploader->refreshCompletedSyncValue();
		if (renderBackendModule != nullptr && renderBackendModule->isBackendCreated())
		{
			RenderBackend* renderBackend = renderBackendModule->getBackend();
			if (renderBackend != nullptr)
			{
				releaseCompletedUploadSubmissions(*renderBackend, gpuUploader->getCompletedUploadSyncValue());
			}
		}
	}

	handleCache.clear();
	pendingGpuUploadHandles.clear();
	inFlightUploadSubmissions.clear();
}

uint32 MeshStreaming::getPendingRequestCount() const
{
	return static_cast<uint32>(pendingGpuUploadHandles.size());
}

string MeshStreaming::getMeshCacheKey(
	const string& meshAssetPath,
	const uint32 lodLevel) const
{
	return meshAssetPath + "|LOD" + std::to_string(lodLevel);
}

void MeshStreaming::releaseCompletedUploadSubmissions(
	RenderBackend& renderBackend,
	const uint64 completedUploadSyncValue)
{
	if (inFlightUploadSubmissions.empty())
	{
		return;
	}

	for (int32 submissionIndex = static_cast<int32>(inFlightUploadSubmissions.size()) - 1; submissionIndex >= 0; --submissionIndex)
	{
		const InFlightUploadSubmission& uploadSubmission = inFlightUploadSubmissions[static_cast<uint32>(submissionIndex)];
		if (uploadSubmission.commandList == nullptr || uploadSubmission.syncValue > completedUploadSyncValue)
		{
			continue;
		}

		renderBackend.releaseCommandList(uploadSubmission.commandList);
		inFlightUploadSubmissions.erase(inFlightUploadSubmissions.begin() + submissionIndex);
	}
}

void MeshStreaming::flushGpuRequests(RenderBackend& renderBackend)
{
	shared_pointer<GPUUploader> gpuUploader = GPUUploader::get();
	assert(gpuUploader != nullptr && "[MeshStreaming][Assert] reason=gpu_uploader_missing");

	gpuUploader->refreshCompletedSyncValue();
	releaseCompletedUploadSubmissions(renderBackend, gpuUploader->getCompletedUploadSyncValue());

	if (pendingGpuUploadHandles.empty() && !gpuUploader->hasQueuedUploadRequests())
	{
		return;
	}

	vector<shared_pointer<MeshAssetHandle>> processingHandles;
	processingHandles.swap(pendingGpuUploadHandles);

	CommandList* uploadCommandList = renderBackend.acquireCommandList();
	assert(uploadCommandList != nullptr && "[MeshStreaming][Assert] reason=gpu_upload_command_list_acquire_failed");

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
		CommandQueue* commandQueue = renderBackend.getCommandQueue();
		assert(commandQueue != nullptr && "[MeshStreaming][Assert] reason=gpu_upload_command_queue_missing");

		uploadCommandList->reset();
		gpuUploader->uploadQueuedBuffers(*uploadCommandList);
		uploadCommandList->close();
		commandQueue->execute(uploadCommandList);

		InFlightUploadSubmission uploadSubmission = {};
		uploadSubmission.commandList = uploadCommandList;
		uploadSubmission.syncValue = gpuUploader->signalUploadSync();
		inFlightUploadSubmissions.push_back(uploadSubmission);
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
		output << "[MeshStreaming][GpuReady] mesh=" << handle->meshAssetPath
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

	shared_pointer<GPUUploader> gpuUploader = GPUUploader::get();
	if (gpuUploader == nullptr)
	{
		failUpload("gpu_uploader_missing");
	}

	if (handle.meshAsset == nullptr)
	{
		failUpload("mesh_asset_missing");
	}

	const MeshAsset& meshAsset = *handle.meshAsset;
	const RawMeshData& rawMeshData = meshAsset.getRawMeshData(handle.lodLevel);
	const uint32 vertexCount = static_cast<uint32>(rawMeshData.positionVertices.size());
	const uint64 indexBufferBytes = static_cast<uint64>(rawMeshData.indices.size()) * sizeof(uint32);
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
			bufferByteSize = static_cast<uint64>(rawMeshData.positionVertices.size()) * sizeof(PositionData);
			initialData = rawMeshData.positionVertices.data();
			elementCount = static_cast<uint32>(rawMeshData.positionVertices.size());
			createFailReason = "position_buffer_create_failed";
			break;
		case MeshBufferSignature::normal:
			bufferByteSize = static_cast<uint64>(rawMeshData.normalVertices.size()) * sizeof(NormalData);
			initialData = rawMeshData.normalVertices.data();
			elementCount = static_cast<uint32>(rawMeshData.normalVertices.size());
			createFailReason = "normal_buffer_create_failed";
			break;
		case MeshBufferSignature::texcoord:
			bufferByteSize = static_cast<uint64>(rawMeshData.texcoordVertices.size()) * sizeof(TexcoordData);
			initialData = rawMeshData.texcoordVertices.data();
			elementCount = static_cast<uint32>(rawMeshData.texcoordVertices.size());
			createFailReason = "texcoord_buffer_create_failed";
			break;
		case MeshBufferSignature::count:
		default:
			failUpload("mesh_buffer_signature_invalid");
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
			failUpload("mesh_data_empty");
		}

		if (bufferByteSize > static_cast<uint64>(uint32MaxValue))
		{
			failUpload("mesh_buffer_size_overflow");
		}

		if (vertexBufferElementCounts[signatureIndex] != vertexCount)
		{
			failUpload("mesh_vertex_stream_mismatch");
		}
	}

	if ((sourceVertexBufferFlags & handle.requiredVertexBufferFlags) != handle.requiredVertexBufferFlags)
	{
		failUpload("mesh_required_vertex_buffer_missing");
	}

	if (indexBufferBytes == 0)
	{
		failUpload("mesh_data_empty");
	}

	if (indexBufferBytes > static_cast<uint64>(uint32MaxValue))
	{
		failUpload("mesh_buffer_size_overflow");
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
			failUpload("mesh_data_empty");
		}

		unique_pointer<BufferResourceObject> createdBufferObject = gpuUploader->createBufferObject(renderBackend, bufferCreateOptions, uploadRequestOptions);
		if (createdBufferObject == nullptr)
		{
			failUpload(vertexBufferCreateFailReasons[signatureIndex]);
		}

		handle.vertexBufferObjects[signatureIndex] = moveValue(createdBufferObject);
		handle.vertexBufferSizesInBytes[signatureIndex] = static_cast<uint32>(bufferCreateOptions.sizeInBytes);
		handle.activeVertexBufferFlags |= signatureFlag;
	}

	if ((handle.activeVertexBufferFlags & handle.requiredVertexBufferFlags) != handle.requiredVertexBufferFlags)
	{
		failUpload("mesh_vertex_upload_flag_mismatch");
	}

	BufferObjectCreateOptions indexBufferCreateOptions = {};
	indexBufferCreateOptions.sizeInBytes = indexBufferBytes;

	BufferUploadRequestOptions indexUploadRequestOptions = {};
	indexUploadRequestOptions.sourceData = rawMeshData.indices.data();
	indexUploadRequestOptions.sourceDataSizeInBytes = indexBufferBytes;
	indexUploadRequestOptions.destinationOffsetInBytes = 0;
	unique_pointer<BufferResourceObject> indexBufferObject = gpuUploader->createBufferObject(renderBackend, indexBufferCreateOptions, indexUploadRequestOptions);
	if (indexBufferObject == nullptr)
	{
		failUpload("index_buffer_create_failed");
	}

	handle.indexBufferObject = moveValue(indexBufferObject);
	handle.indexBufferSizeInBytes = static_cast<uint32>(indexBufferBytes);

	if (handle.indexBufferObject == nullptr || handle.indexBufferSizeInBytes == 0)
	{
		failUpload("mesh_index_upload_invalid");
	}

	return true;
}
