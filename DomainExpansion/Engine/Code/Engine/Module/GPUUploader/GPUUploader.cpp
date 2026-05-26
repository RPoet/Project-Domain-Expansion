#include "Engine/Module/GPUUploader/GPUUploader.h"

#include "Engine/Framework/Framework.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Render/Backends/CommandList.h"
#include "Render/Backends/RenderBackend.h"

GPUUploader::GPUUploader()
	: StaticModule("GPUUploader")
{
}

GPUUploader::~GPUUploader() = default;

void GPUUploader::UploadHeapBuffer::initialize(unique_pointer<BufferResourceObject>&& inBufferObject, char* inMappedMemory, const uint64 inCapacityInBytes)
{
	bufferObject = moveValue(inBufferObject);
	mappedMemory = inMappedMemory;
	capacityInBytes = inCapacityInBytes;
	usedInBytes = 0;
}

bool GPUUploader::UploadHeapBuffer::isValid() const
{
	return bufferObject != nullptr && mappedMemory != nullptr && capacityInBytes != 0;
}

bool GPUUploader::UploadHeapBuffer::canAllocate(const uint64 requestSizeInBytes) const
{
	return isValid()
		&& capacityInBytes >= usedInBytes
		&& capacityInBytes - usedInBytes >= requestSizeInBytes;
}

GPUUploader::UploadHeapBufferAllocation GPUUploader::UploadHeapBuffer::allocate(const uint64 requestSizeInBytes)
{
	assert(canAllocate(requestSizeInBytes) && "[GPUUploader][Assert] reason=upload_heap_buffer_allocate_failed");

	UploadHeapBufferAllocation allocation
	{
		.bufferObject = bufferObject.get(),
		.mappedMemory = mappedMemory + static_cast<size_t>(usedInBytes),
		.offsetInBytes = usedInBytes,
		.sizeInBytes = requestSizeInBytes,
	};
	usedInBytes += requestSizeInBytes;
	return allocation;
}

void GPUUploader::UploadHeapBuffer::resetAllocations()
{
	usedInBytes = 0;
}

void GPUUploader::UploadHeapBuffer::clear()
{
	if (bufferObject != nullptr)
	{
		bufferObject->unmap(0);
	}

	bufferObject.reset();
	mappedMemory = nullptr;
	capacityInBytes = 0;
	usedInBytes = 0;
}

bool GPUUploader::initialize(Framework& framework)
{
	unused(framework);
	uploaderMode = GPUUploaderMode::staging;
	bufferAllocator.initialize(GPUBufferAllocatorCreateOptions{});
	completedUploadSyncValue = 0;
	lastSubmittedUploadSyncValue = 0;
	queuedUploadRequestSizeInBytes = 0;
	uploadSyncObject.reset();

	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	if (renderBackendModule->isBackendCreated())
	{
		RenderBackend* renderBackend = renderBackendModule->getBackend();
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

	recycleCompletedUploadBuffers();
}

void GPUUploader::postUpdate()
{
	releaseIdlePoolBlocks();
}

void GPUUploader::shutdown()
{
	clearPool();
	bufferAllocator.clear();
	completedUploadSyncValue = 0;
	lastSubmittedUploadSyncValue = 0;
	queuedUploadRequestSizeInBytes = 0;
	uploadSyncObject.reset();
}

unique_pointer<BufferResourceObject> GPUUploader::createBufferObject(
	RenderBackend& renderBackend,
	const BufferObjectCreateOptions& createOptions,
	const BufferUploadRequestOptions& uploadRequest)
{
	PROFILE_SCOPE("GPUUploader", "createBufferObject");
	assert(uploadRequest.sourceDataSizeInBytes != 0);

	UploadHeapBufferAllocation sourceAllocation = {};
	reserveUploadSpace(renderBackend, uploadRequest.sourceDataSizeInBytes, sourceAllocation);
	assert(sourceAllocation.mappedMemory && "[GPUUploader][Assert] reason=upload_pool_reserve_failed");
	memcpy(sourceAllocation.mappedMemory, uploadRequest.sourceData, static_cast<size_t>(uploadRequest.sourceDataSizeInBytes));

	unique_pointer<BufferResourceObject> createdBufferObject = bufferAllocator.allocate(renderBackend, createOptions);
	QueuedUploadRequest queuedUploadRequest
	{
		.destinationBufferObject = createdBufferObject.get(),
		.sourceBufferObject = sourceAllocation.bufferObject,
		.destinationOffsetInBytes = uploadRequest.destinationOffsetInBytes,
		.sourceOffsetInBytes = sourceAllocation.offsetInBytes,
		.copySizeInBytes = sourceAllocation.sizeInBytes,
	};
	queuedUploadRequests.push_back(moveValue(queuedUploadRequest));
	queuedUploadRequestSizeInBytes += sourceAllocation.sizeInBytes;
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

void GPUUploader::recycleCompletedUploadBuffers()
{
	for (uint32 blockIndex = 0; blockIndex < static_cast<uint32>(uploadBufferPoolBlocks.size()); ++blockIndex)
	{
		UploadBufferPoolBlock& block = uploadBufferPoolBlocks[blockIndex];
		if (block.pendingSubmission || (block.lastUsedSyncValue != 0 && block.lastUsedSyncValue > completedUploadSyncValue))
		{
			continue;
		}

		block.uploadBuffer.resetAllocations();
	}
}

void GPUUploader::uploadQueuedBuffers(CommandList& commandList)
{
	PROFILE_SCOPE("GPUUploader", "uploadQueuedBuffers");

	for (uint32 requestIndex = 0; requestIndex < static_cast<uint32>(queuedUploadRequests.size()); ++requestIndex)
	{
		const QueuedUploadRequest& queuedUploadRequest = queuedUploadRequests[requestIndex];
		commandList.copyBuffer
		(
			queuedUploadRequest.destinationBufferObject,
			queuedUploadRequest.destinationOffsetInBytes,
			queuedUploadRequest.sourceBufferObject,
			queuedUploadRequest.sourceOffsetInBytes,
			queuedUploadRequest.copySizeInBytes
		);
	}

	queuedUploadRequests.clear();
	queuedUploadRequestSizeInBytes = 0;
}

bool GPUUploader::hasQueuedUploadRequests() const
{
	return !queuedUploadRequests.empty();
}

bool GPUUploader::isQueuedUploadRequestThresholdReached() const
{
	return queuedUploadRequestSizeInBytes >= queuedUploadSubmitThresholdInBytes;
}

uint64 GPUUploader::getQueuedUploadRequestSizeInBytes() const
{
	return queuedUploadRequestSizeInBytes;
}

void GPUUploader::reserveUploadSpace(RenderBackend& renderBackend, const uint64 requestSizeInBytes, UploadHeapBufferAllocation& outAllocation)
{
	PROFILE_SCOPE("GPUUploader", "reserveUploadSpace");
	outAllocation = {};

	for (uint32 blockIndex = 0; blockIndex < static_cast<uint32>(uploadBufferPoolBlocks.size()); ++blockIndex)
	{
		UploadBufferPoolBlock& block = uploadBufferPoolBlocks[blockIndex];
		if (!block.uploadBuffer.canAllocate(requestSizeInBytes))
		{
			continue;
		}

		outAllocation = block.uploadBuffer.allocate(requestSizeInBytes);
		block.pendingSubmission = true;
		return;
	}

	{
		PROFILE_SCOPE("GPUUploader", "createUploadPoolBlock");
		const uint64 poolBlockSizeInBytes = requestSizeInBytes > defaultPoolBlockSizeInBytes ? roundUpToPowerOfTwo(requestSizeInBytes) : defaultPoolBlockSizeInBytes;

		BufferObjectCreateOptions uploadBufferCreateOptions
		{
			.sizeInBytes = poolBlockSizeInBytes,
			.memoryType = BufferObjectMemoryType::uploadHeap,
		};
		unique_pointer<BufferResourceObject> uploadBufferObject = renderBackend.createBufferObject(uploadBufferCreateOptions);
		assert(uploadBufferObject != nullptr && "[GPUUploader][Assert] reason=upload_pool_block_create_failed");

		MapRange readRange = { .start = 0, .end = 0 };
		void* mappedMemory = uploadBufferObject->map(0, readRange);

		UploadBufferPoolBlock uploadBufferPoolBlock;
		uploadBufferPoolBlock.uploadBuffer.initialize(moveValue(uploadBufferObject), static_cast<char*>(mappedMemory), poolBlockSizeInBytes);
		uploadBufferPoolBlock.lastUsedSyncValue = lastSubmittedUploadSyncValue;
		uploadBufferPoolBlock.pendingSubmission = false;
		uploadBufferPoolBlocks.push_back(moveValue(uploadBufferPoolBlock));

		UploadBufferPoolBlock& block = uploadBufferPoolBlocks[static_cast<uint32>(uploadBufferPoolBlocks.size() - 1)];
		outAllocation = block.uploadBuffer.allocate(requestSizeInBytes);
		block.pendingSubmission = true;
	}
}

void GPUUploader::clearUploadBufferBlock(UploadBufferPoolBlock& poolBlock)
{
	poolBlock.uploadBuffer.clear();
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

void GPUUploader::clearPool()
{
	for (uint32 blockIndex = 0; blockIndex < static_cast<uint32>(uploadBufferPoolBlocks.size()); ++blockIndex)
	{
		clearUploadBufferBlock(uploadBufferPoolBlocks[blockIndex]);
	}

	uploadBufferPoolBlocks.clear();
	queuedUploadRequests.clear();
	queuedUploadRequestSizeInBytes = 0;
}

void GPUUploader::initializeUploadSyncObject(RenderBackend& renderBackend)
{
	uploadSyncObject = renderBackend.createSyncObject();
	completedUploadSyncValue = 0;
	lastSubmittedUploadSyncValue = 0;
	queuedUploadRequestSizeInBytes = 0;
}
