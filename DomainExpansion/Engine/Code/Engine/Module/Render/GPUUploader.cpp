#include "Engine/Module/Render/GPUUploader.h"

#include "Engine/Module/Render/RenderBackendModule.h"
#include "Render/CommandList.h"
#include "Render/Backends/RenderBackend.h"

#include "Engine/Profiler/ProfilerScope.h"

#include <cstring>

bool GPUUploader::init(Framework& framework)
{
	unused(framework);
	uploaderMode = GPUUploaderMode::staging;
	completedUploadSyncValue = 0;
	lastSubmittedUploadSyncValue = 0;
	uploadSyncObject.reset();

	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	if (renderBackendModule->isBackendCreated())
	{
		RenderBackend* renderBackend = renderBackendModule->getBackend();
		assert(renderBackend != nullptr && "[GPUUploader][Assert] reason=render_backend_missing");
		initializeUploadSyncObject(*renderBackend);
	}

	clearPool();
	return true;
}

void GPUUploader::preUpdate()
{
	if (uploadSyncObject != nullptr)
	{
		refreshCompletedSyncValue();
	}

	resetFrameAllocations();
}

void GPUUploader::postUpdate()
{
	releaseIdlePoolBlocks();
}

void GPUUploader::shutdown()
{
	clearPool();
	completedUploadSyncValue = 0;
	lastSubmittedUploadSyncValue = 0;
	uploadSyncObject.reset();
}

unique_pointer<BufferResourceObject> GPUUploader::createBufferObject(
	RenderBackend& renderBackend,
	const BufferObjectCreateOptions& createOptions,
	const BufferUploadRequestOptions& uploadRequestOptions)
{
	PROFILE_SCOPE("MeshStreaming", "createBufferObject");
	const bool validCreateOptions = createOptions.sizeInBytes != 0;
	assert(validCreateOptions && "[GPUUploader][Assert] reason=invalid_buffer_create_options");

	const bool validUploadRequest =
		uploadRequestOptions.sourceData != nullptr
		&& uploadRequestOptions.sourceDataSizeInBytes != 0;
	assert(validUploadRequest && "[GPUUploader][Assert] reason=invalid_upload_request");

	const bool uploadRequestInRange =
		uploadRequestOptions.destinationOffsetInBytes + uploadRequestOptions.sourceDataSizeInBytes
		<= createOptions.sizeInBytes;
	assert(uploadRequestInRange && "[GPUUploader][Assert] reason=upload_request_out_of_range");

	uint32 poolBlockIndex = uint32MaxValue;
	uint64 sourceOffsetInBytes = 0;
	const bool reservedUploadSpace = reserveUploadSpace(
		renderBackend,
		uploadRequestOptions.sourceDataSizeInBytes,
		poolBlockIndex,
		sourceOffsetInBytes);
	assert(reservedUploadSpace && "[GPUUploader][Assert] reason=upload_pool_reserve_failed");

	const bool validPoolBlockIndex = poolBlockIndex < static_cast<uint32>(uploadBufferPoolBlocks.size());
	assert(validPoolBlockIndex && "[GPUUploader][Assert] reason=upload_pool_block_index_invalid");

	UploadBufferPoolBlock& uploadBufferPoolBlock = uploadBufferPoolBlocks[poolBlockIndex];
	const bool validUploadPoolBlock =
		uploadBufferPoolBlock.bufferObject != nullptr
		&& uploadBufferPoolBlock.mappedMemory != nullptr;
	assert(validUploadPoolBlock && "[GPUUploader][Assert] reason=upload_pool_block_missing");

	std::memcpy(
		uploadBufferPoolBlock.mappedMemory + static_cast<size_t>(sourceOffsetInBytes),
		uploadRequestOptions.sourceData,
		static_cast<size_t>(uploadRequestOptions.sourceDataSizeInBytes));

	BufferObjectCreateOptions destinationBufferCreateOptions = createOptions;
	destinationBufferCreateOptions.memoryType = BufferObjectMemoryType::defaultHeap;
	unique_pointer<BufferResourceObject> createdBufferObject = renderBackend.createBufferObject(destinationBufferCreateOptions);
	assert(createdBufferObject != nullptr && "[GPUUploader][Assert] reason=backend_create_buffer_failed");

	QueuedUploadRequest queuedUploadRequest = {};
	queuedUploadRequest.destinationBufferObject = createdBufferObject.get();
	queuedUploadRequest.sourceBufferObject = uploadBufferPoolBlock.bufferObject.get();
	queuedUploadRequest.destinationOffsetInBytes = uploadRequestOptions.destinationOffsetInBytes;
	queuedUploadRequest.sourceOffsetInBytes = sourceOffsetInBytes;
	queuedUploadRequest.copySizeInBytes = uploadRequestOptions.sourceDataSizeInBytes;
	queuedUploadRequests.push_back(moveValue(queuedUploadRequest));

	return createdBufferObject;
}

void GPUUploader::refreshCompletedSyncValue()
{
	assert(uploadSyncObject != nullptr && "[GPUUploader][Assert] reason=upload_sync_object_missing");
	completedUploadSyncValue = uploadSyncObject->getCompletedSyncValue();
}

uint64 GPUUploader::signalUploadSync()
{
	assert(uploadSyncObject != nullptr && "[GPUUploader][Assert] reason=upload_sync_object_missing");
	const uint64 submittedSyncValue = uploadSyncObject->signal();
	assert(submittedSyncValue != 0 && "[GPUUploader][Assert] reason=upload_sync_signal_failed");

	lastSubmittedUploadSyncValue = submittedSyncValue;
	for (uint32 blockIndex = 0; blockIndex < static_cast<uint32>(uploadBufferPoolBlocks.size()); ++blockIndex)
	{
		UploadBufferPoolBlock& block = uploadBufferPoolBlocks[blockIndex];
		if (!block.pendingSubmission)
		{
			continue;
		}

		block.lastUsedSyncValue = submittedSyncValue;
		block.pendingSubmission = false;
	}

	return submittedSyncValue;
}

uint64 GPUUploader::getCompletedUploadSyncValue() const
{
	return completedUploadSyncValue;
}

void GPUUploader::waitForUploadCompletion()
{
	assert(uploadSyncObject != nullptr && "[GPUUploader][Assert] reason=upload_sync_object_missing");
	uploadSyncObject->wait();
	completedUploadSyncValue = uploadSyncObject->getCompletedSyncValue();
}

void GPUUploader::uploadQueuedBuffers(CommandList& commandList)
{
	if (queuedUploadRequests.empty())
	{
		return;
	}

	for (uint32 requestIndex = 0; requestIndex < static_cast<uint32>(queuedUploadRequests.size()); ++requestIndex)
	{
		const QueuedUploadRequest& queuedUploadRequest = queuedUploadRequests[requestIndex];
		unused(requestIndex);
		const bool validQueuedUploadRequest =
			queuedUploadRequest.destinationBufferObject != nullptr
			&& queuedUploadRequest.sourceBufferObject != nullptr
			&& queuedUploadRequest.copySizeInBytes != 0;
		assert(validQueuedUploadRequest && "[GPUUploader][Assert] reason=queued_upload_request_invalid");

		commandList.copyBuffer(
			queuedUploadRequest.destinationBufferObject,
			queuedUploadRequest.destinationOffsetInBytes,
			queuedUploadRequest.sourceBufferObject,
			queuedUploadRequest.sourceOffsetInBytes,
			queuedUploadRequest.copySizeInBytes);
	}

	queuedUploadRequests.clear();
}

bool GPUUploader::hasQueuedUploadRequests() const
{
	return !queuedUploadRequests.empty();
}

bool GPUUploader::reserveUploadSpace(
	RenderBackend& renderBackend,
	const uint64 requestSizeInBytes,
	uint32& outBlockIndex,
	uint64& outBlockOffsetInBytes)
{
	outBlockIndex = uint32MaxValue;
	outBlockOffsetInBytes = 0;

	if (requestSizeInBytes == 0)
	{
		return true;
	}

	for (uint32 blockIndex = 0; blockIndex < static_cast<uint32>(uploadBufferPoolBlocks.size()); ++blockIndex)
	{
		UploadBufferPoolBlock& block = uploadBufferPoolBlocks[blockIndex];
		if (block.bufferObject == nullptr || block.mappedMemory == nullptr)
		{
			continue;
		}

		if (block.capacityInBytes < block.usedInBytes
			|| block.capacityInBytes - block.usedInBytes < requestSizeInBytes)
		{
			continue;
		}

		outBlockIndex = blockIndex;
		outBlockOffsetInBytes = block.usedInBytes;
		block.usedInBytes += requestSizeInBytes;
		block.pendingSubmission = true;
		return true;
	}

	if (!createUploadPoolBlock(renderBackend, requestSizeInBytes))
	{
		return false;
	}

	if (uploadBufferPoolBlocks.empty())
	{
		return false;
	}

	outBlockIndex = static_cast<uint32>(uploadBufferPoolBlocks.size() - 1);
	UploadBufferPoolBlock& block = uploadBufferPoolBlocks[outBlockIndex];
	outBlockOffsetInBytes = 0;
	block.usedInBytes = requestSizeInBytes;
	block.pendingSubmission = true;
	return true;
}

bool GPUUploader::createUploadPoolBlock(RenderBackend& renderBackend, const uint64 requestSizeInBytes)
{
	PROFILE_SCOPE("GPUUploader", "createUploadPoolBlock");
	const uint64 poolBlockSizeInBytes = roundUpToPowerOfTwo(requestSizeInBytes > minimumPoolBlockSizeInBytes ? requestSizeInBytes : minimumPoolBlockSizeInBytes);

	BufferObjectCreateOptions uploadBufferCreateOptions = {};
	uploadBufferCreateOptions.sizeInBytes = poolBlockSizeInBytes;
	uploadBufferCreateOptions.memoryType = BufferObjectMemoryType::uploadHeap;
	unique_pointer<BufferResourceObject> uploadBufferObject = renderBackend.createBufferObject(uploadBufferCreateOptions);
	assert(uploadBufferObject != nullptr && "[GPUUploader][Assert] reason=upload_pool_block_create_failed");

	// REFACTOR_BEGIN | Why the GPU uploader knows the detail of the resource in here? terrible implmentation.
	// GPU uploader must not know specific implementation of each backend.
	MapRange readRange = { .start = 0, .end = 0 };
	void* mappedMemory = uploadBufferObject->map(0, readRange);
	// REFACTOR_END

	UploadBufferPoolBlock uploadBufferPoolBlock = {};
	uploadBufferPoolBlock.bufferObject = moveValue(uploadBufferObject);
	uploadBufferPoolBlock.mappedMemory = static_cast<char*>(mappedMemory);
	uploadBufferPoolBlock.capacityInBytes = poolBlockSizeInBytes;
	uploadBufferPoolBlock.usedInBytes = 0;
	uploadBufferPoolBlock.lastUsedSyncValue = lastSubmittedUploadSyncValue;
	uploadBufferPoolBlock.pendingSubmission = false;
	uploadBufferPoolBlocks.push_back(moveValue(uploadBufferPoolBlock));
	return true;
}

void GPUUploader::clearUploadBufferBlock(UploadBufferPoolBlock& poolBlock)
{
	if (poolBlock.bufferObject != nullptr)
	{
		poolBlock.bufferObject->unmap(0);
	}

	poolBlock.bufferObject.reset();
	poolBlock.mappedMemory = nullptr;
	poolBlock.capacityInBytes = 0;
	poolBlock.usedInBytes = 0;
	poolBlock.lastUsedSyncValue = 0;
	poolBlock.pendingSubmission = false;
}

void GPUUploader::releaseIdlePoolBlocks()
{
	if (uploadBufferPoolBlocks.empty())
	{
		return;
	}

	for (int32 blockIndex = static_cast<int32>(uploadBufferPoolBlocks.size()) - 1; blockIndex >= 0; --blockIndex)
	{
		UploadBufferPoolBlock& block = uploadBufferPoolBlocks[static_cast<uint32>(blockIndex)];
		if (block.pendingSubmission
			|| block.lastUsedSyncValue + idleReleaseSyncValueThreshold >= completedUploadSyncValue)
		{
			continue;
		}

		clearUploadBufferBlock(block);
		uploadBufferPoolBlocks.erase(uploadBufferPoolBlocks.begin() + blockIndex);
	}
}

void GPUUploader::resetFrameAllocations()
{
	for (uint32 blockIndex = 0; blockIndex < static_cast<uint32>(uploadBufferPoolBlocks.size()); ++blockIndex)
	{
		UploadBufferPoolBlock& block = uploadBufferPoolBlocks[blockIndex];
		if (block.pendingSubmission
			|| (block.lastUsedSyncValue != 0 && block.lastUsedSyncValue > completedUploadSyncValue))
		{
			continue;
		}

		block.usedInBytes = 0;
	}
}

void GPUUploader::clearPool()
{
	for (uint32 blockIndex = 0; blockIndex < static_cast<uint32>(uploadBufferPoolBlocks.size()); ++blockIndex)
	{
		clearUploadBufferBlock(uploadBufferPoolBlocks[blockIndex]);
	}

	uploadBufferPoolBlocks.clear();
	queuedUploadRequests.clear();
}

void GPUUploader::initializeUploadSyncObject(RenderBackend& renderBackend)
{
	assert(uploadSyncObject == nullptr && "[GPUUploader][Assert] reason=upload_sync_object_already_initialized");
	uploadSyncObject = renderBackend.createSyncObject();
	assert(uploadSyncObject != nullptr && "[GPUUploader][Assert] reason=upload_sync_object_create_failed");
	completedUploadSyncValue = 0;
	lastSubmittedUploadSyncValue = 0;
}
