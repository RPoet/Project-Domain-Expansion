#include "Engine/Module/MeshStreaming/MeshStreaming.h"

#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/GPUUploader/GPUUploader.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Render/Backends/RenderBackend.h"

static void initializeMeshAssetHandleVertexBuffers(MeshAssetHandle& handle)
{
	const uint32 defaultRequiredVertexBufferFlags = getMeshBufferSignatureFlag(MeshBufferSignature::position)
		| getMeshBufferSignatureFlag(MeshBufferSignature::normal)
		| getMeshBufferSignatureFlag(MeshBufferSignature::texcoord);

	if (handle.requiredVertexBufferFlags == 0)
	{
		handle.requiredVertexBufferFlags = defaultRequiredVertexBufferFlags;
	}
	assert((handle.requiredVertexBufferFlags & ~defaultRequiredVertexBufferFlags) == 0 && "[MeshStreaming][Assert] reason=mesh_required_vertex_buffer_flags_invalid");

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

// TO DO : calculate mesh LOD here and upload its LOD onto GPU
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
	if (!inFlightUploadSubmissions.empty())
	{
		shared_pointer<GPUUploader> gpuUploader = GPUUploader::get();
		assert(gpuUploader != nullptr && "[MeshStreaming][Assert] reason=gpu_uploader_missing");

		gpuUploader->waitForUploadCompletion();
		gpuUploader->refreshCompletedSyncValue();

		shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
		assert(renderBackendModule != nullptr && "[MeshStreaming][Assert] reason=render_backend_module_missing");
		assert(renderBackendModule->isBackendCreated() && "[MeshStreaming][Assert] reason=render_backend_missing");

		RenderBackend* renderBackend = renderBackendModule->getBackend();
		assert(renderBackend != nullptr && "[MeshStreaming][Assert] reason=render_backend_missing");
		releaseCompletedUploadSubmissions(*renderBackend, *gpuUploader, gpuUploader->getCompletedUploadSyncValue());
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
	return meshAssetPath + "|LOD" + to_string(lodLevel);
}

void MeshStreaming::releaseCompletedUploadSubmissions(
	RenderBackend& renderBackend,
	GPUUploader& gpuUploader,
	const uint64 completedUploadSyncValue)
{
	bool releasedAnySubmission = false;
	for (int32 submissionIndex = static_cast<int32>(inFlightUploadSubmissions.size()) - 1; submissionIndex >= 0; --submissionIndex)
	{
		const InFlightUploadSubmission& uploadSubmission = inFlightUploadSubmissions[static_cast<uint32>(submissionIndex)];
		if (uploadSubmission.commandList == nullptr || uploadSubmission.syncValue > completedUploadSyncValue)
		{
			continue;
		}

		renderBackend.releaseCommandList(uploadSubmission.commandList);
		inFlightUploadSubmissions.erase(inFlightUploadSubmissions.begin() + submissionIndex);
		releasedAnySubmission = true;
	}

	if (releasedAnySubmission)
	{
		gpuUploader.recycleCompletedUploadBuffers();
	}
}

// Refactor this function
void MeshStreaming::flushGpuRequests(RenderBackend& renderBackend)
{
	PROFILE_SCOPE("MeshStreaming", "flushGpuRequests");
	shared_pointer<GPUUploader> gpuUploader = GPUUploader::get();

	uint64 completedUploadSyncValue = gpuUploader->getCompletedUploadSyncValue();
	releaseCompletedUploadSubmissions(renderBackend, *gpuUploader, completedUploadSyncValue);

	if (pendingGpuUploadHandles.empty() && !gpuUploader->hasQueuedUploadRequests())
	{
		return;
	}

	CommandQueue* commandQueue = renderBackend.getCommandQueue();
	const CommandListType uploadCommandListType = renderBackend.supportsCommandListType(CommandListType::copy)
		? CommandListType::copy
		: CommandListType::graphics;

	auto flushCommandsToGPU = [&](const bool forceFlush)
	{
		if (!gpuUploader->hasQueuedUploadRequests()
			|| (!forceFlush && !gpuUploader->isQueuedUploadRequestThresholdReached()))
		{
			return;
		}

		CommandList* uploadCommandList = renderBackend.acquireCommandList(uploadCommandListType);
		assert(uploadCommandList != nullptr && "[MeshStreaming][Assert] reason=gpu_upload_command_list_acquire_failed");

		uploadCommandList->reset();
		gpuUploader->uploadQueuedBuffers(*uploadCommandList);
		uploadCommandList->close();
		commandQueue->execute(uploadCommandList);

		InFlightUploadSubmission uploadSubmission = {
			.commandList = uploadCommandList,
			.syncValue = gpuUploader->signalUploadSync(),
		};
		inFlightUploadSubmissions.push_back(uploadSubmission);
	};

	vector<shared_pointer<MeshAssetHandle>> processingHandles;
	processingHandles.swap(pendingGpuUploadHandles);

	vector<shared_pointer<MeshAssetHandle>> uploadedHandles;
	for (uint32 handleIndex = 0; handleIndex < static_cast<uint32>(processingHandles.size()); ++handleIndex)
	{
		gpuUploader->refreshCompletedSyncValue();
		completedUploadSyncValue = gpuUploader->getCompletedUploadSyncValue();
		releaseCompletedUploadSubmissions(renderBackend, *gpuUploader, completedUploadSyncValue);

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
		flushCommandsToGPU(false);
	}

	flushCommandsToGPU(true);
	if (!uploadedHandles.empty())
	{
		gpuUploader->waitForUploadCompletion();
		completedUploadSyncValue = gpuUploader->getCompletedUploadSyncValue();
		releaseCompletedUploadSubmissions(renderBackend, *gpuUploader, completedUploadSyncValue);
	}

	for (uint32 handleIndex = 0; handleIndex < static_cast<uint32>(uploadedHandles.size()); ++handleIndex)
	{
		shared_pointer<MeshAssetHandle>& handle = uploadedHandles[handleIndex];
		if (handle == nullptr)
		{
			continue;
		}

		handle->gpuState = MeshAssetGpuState::ready;
		//output << "[MeshStreaming][GpuReady] mesh=" << handle->meshAssetPath
		//	   << " lod=" << handle->lodLevel
		//	   << " positionBufferBytes=" << handle->getBufferSizeInBytes(MeshBufferSignature::position)
		//	   << " normalBufferBytes=" << handle->getBufferSizeInBytes(MeshBufferSignature::normal)
		//	   << " texcoordBufferBytes=" << handle->getBufferSizeInBytes(MeshBufferSignature::texcoord)
		//	   << " indexBufferBytes=" << handle->indexBufferSizeInBytes
		//	   << " activeVertexBufferFlags=" << handle->activeVertexBufferFlags << lineBreak;
	}
}

bool MeshStreaming::uploadMeshHandleToGpu(
	RenderBackend& renderBackend,
	MeshAssetHandle& handle) const
{
	PROFILE_SCOPE("MeshStreaming", "uploadMeshHandleToGpu");
	initializeMeshAssetHandleVertexBuffers(handle);

	shared_pointer<GPUUploader> gpuUploader = GPUUploader::get();

	const MeshAsset& meshAsset = *handle.meshAsset;
	const RawMeshData& rawMeshData = meshAsset.getRawMeshData(handle.lodLevel);
	const uint64 indexBufferBytes = static_cast<uint64>(rawMeshData.indices.size()) * sizeof(uint32);
	uint64 vertexBufferSizesInBytes[meshVertexBufferSignatureCount] = {};
	const void* vertexBufferInitialData[meshVertexBufferSignatureCount] = {};

	for (uint32 signatureIndex = 0; signatureIndex < meshVertexBufferSignatureCount; ++signatureIndex)
	{
		const MeshBufferSignature signature = static_cast<MeshBufferSignature>(signatureIndex);
		uint64 bufferByteSize = 0;
		const void* initialData = nullptr;

		switch (signature)
		{
		case MeshBufferSignature::position:
			bufferByteSize = static_cast<uint64>(rawMeshData.positionVertices.size()) * sizeof(PositionData);
			initialData = rawMeshData.positionVertices.data();
			break;
		case MeshBufferSignature::normal:
			bufferByteSize = static_cast<uint64>(rawMeshData.normalVertices.size()) * sizeof(NormalData);
			initialData = rawMeshData.normalVertices.data();
			break;
		case MeshBufferSignature::texcoord:
			bufferByteSize = static_cast<uint64>(rawMeshData.texcoordVertices.size()) * sizeof(TexcoordData);
			initialData = rawMeshData.texcoordVertices.data();
			break;
		case MeshBufferSignature::count:
		default:
			assert(false && "[MeshStreaming][Assert] reason=mesh_buffer_signature_invalid");
			break;
		}

		vertexBufferSizesInBytes[signatureIndex] = bufferByteSize;
		vertexBufferInitialData[signatureIndex] = initialData;
	}

	for (uint32 signatureIndex = 0; signatureIndex < meshVertexBufferSignatureCount; ++signatureIndex)
	{
		const MeshBufferSignature signature = static_cast<MeshBufferSignature>(signatureIndex);
		const uint32 signatureFlag = getMeshBufferSignatureFlag(signature);
		if ((handle.requiredVertexBufferFlags & signatureFlag) != 0)
		{
			BufferObjectCreateOptions bufferCreateOptions = {
				.sizeInBytes = vertexBufferSizesInBytes[signatureIndex],
			};

			BufferUploadRequestOptions uploadRequestOptions = {
				.sourceData = vertexBufferInitialData[signatureIndex],
				.sourceDataSizeInBytes = vertexBufferSizesInBytes[signatureIndex],
				.destinationOffsetInBytes = 0,
			};

			unique_pointer<BufferResourceObject> createdBufferObject = gpuUploader->createBufferObject(renderBackend, bufferCreateOptions, uploadRequestOptions);
			handle.vertexBufferObjects[signatureIndex] = moveValue(createdBufferObject);
			handle.vertexBufferSizesInBytes[signatureIndex] = static_cast<uint32>(bufferCreateOptions.sizeInBytes);
			handle.activeVertexBufferFlags |= signatureFlag;
		}
	}
	assert((handle.activeVertexBufferFlags & handle.requiredVertexBufferFlags) == handle.requiredVertexBufferFlags && "[MeshStreaming][Assert] reason=mesh_vertex_upload_flag_mismatch");

	BufferObjectCreateOptions indexBufferCreateOptions = {
		.sizeInBytes = indexBufferBytes,
	};

	BufferUploadRequestOptions indexUploadRequestOptions = {
		.sourceData = rawMeshData.indices.data(),
		.sourceDataSizeInBytes = indexBufferBytes,
		.destinationOffsetInBytes = 0,
	};
	unique_pointer<BufferResourceObject> indexBufferObject = gpuUploader->createBufferObject(renderBackend, indexBufferCreateOptions, indexUploadRequestOptions);

	handle.indexBufferObject = moveValue(indexBufferObject);
	handle.indexBufferSizeInBytes = static_cast<uint32>(indexBufferBytes);

	return true;
}
