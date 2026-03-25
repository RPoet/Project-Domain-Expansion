#include "Engine/Module/Render/GPUUploader.h"

#include "Engine/Module/Render/RenderBackendModule.h"
#include "Render/CommandList.h"
#include "Render/Backends/RenderBackend.h"

#include <d3d12.h>
#include <cstring>

bool GPUUploader::init(Framework& framework)
{
	unused(framework);
	uploaderMode = GPUUploaderMode::staging;
	completedUploadSyncValue = 0;
	lastSubmittedUploadSyncValue = 0;
	uploadSyncObject.reset();

	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	const bool validRenderBackendModule = renderBackendModule != nullptr;
	assert(validRenderBackendModule && "[GPUUploader][Assert] reason=render_backend_module_missing");
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
	if (createOptions.sizeInBytes == 0)
	{
		error << "[GPUUploader][Error] reason=invalid_buffer_create_options sizeInBytes="
			  << createOptions.sizeInBytes << lineBreak;
		return nullptr;
	}

	if (uploadRequestOptions.sourceData == nullptr || uploadRequestOptions.sourceDataSizeInBytes == 0)
	{
		error << "[GPUUploader][Error] reason=invalid_upload_request sourceDataSizeInBytes="
			  << uploadRequestOptions.sourceDataSizeInBytes << lineBreak;
		return nullptr;
	}

	if (uploadRequestOptions.destinationOffsetInBytes + uploadRequestOptions.sourceDataSizeInBytes > createOptions.sizeInBytes)
	{
		error << "[GPUUploader][Error] reason=upload_request_out_of_range sizeInBytes="
			  << createOptions.sizeInBytes << " destinationOffsetInBytes="
			  << uploadRequestOptions.destinationOffsetInBytes << " sourceDataSizeInBytes="
			  << uploadRequestOptions.sourceDataSizeInBytes << lineBreak;
		return nullptr;
	}

	uint32 poolBlockIndex = uint32MaxValue;
	uint64 sourceOffsetInBytes = 0;
	if (!reserveUploadSpace(
		renderBackend,
		uploadRequestOptions.sourceDataSizeInBytes,
		poolBlockIndex,
		sourceOffsetInBytes))
	{
		error << "[GPUUploader][Error] reason=upload_pool_reserve_failed sourceDataSizeInBytes="
			  << uploadRequestOptions.sourceDataSizeInBytes << lineBreak;
		return nullptr;
	}

	if (poolBlockIndex >= static_cast<uint32>(uploadBufferPoolBlocks.size()))
	{
		error << "[GPUUploader][Error] reason=upload_pool_block_index_invalid poolBlockIndex="
			  << poolBlockIndex << lineBreak;
		return nullptr;
	}

	UploadBufferPoolBlock& uploadBufferPoolBlock = uploadBufferPoolBlocks[poolBlockIndex];
	if (uploadBufferPoolBlock.bufferObject == nullptr || uploadBufferPoolBlock.mappedMemory == nullptr)
	{
		error << "[GPUUploader][Error] reason=upload_pool_block_missing" << lineBreak;
		return nullptr;
	}

	std::memcpy(
		uploadBufferPoolBlock.mappedMemory + static_cast<size_t>(sourceOffsetInBytes),
		uploadRequestOptions.sourceData,
		static_cast<size_t>(uploadRequestOptions.sourceDataSizeInBytes));

	BufferObjectCreateOptions destinationBufferCreateOptions = createOptions;
	destinationBufferCreateOptions.memoryType = BufferObjectMemoryType::defaultHeap;
	unique_pointer<BufferResourceObject> createdBufferObject = renderBackend.createBufferObject(destinationBufferCreateOptions);
	if (createdBufferObject == nullptr)
	{
		error << "[GPUUploader][Error] reason=backend_create_buffer_failed sizeInBytes="
			  << createOptions.sizeInBytes << lineBreak;
		return nullptr;
	}

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

void GPUUploader::signalUploadSync()
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
		if (queuedUploadRequest.destinationBufferObject == nullptr
			|| queuedUploadRequest.sourceBufferObject == nullptr
			|| queuedUploadRequest.copySizeInBytes == 0)
		{
			error << "[GPUUploader][Error] reason=queued_upload_request_invalid requestIndex="
				  << requestIndex << lineBreak;
			continue;
		}

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
	const uint64 poolBlockSizeInBytes =
		roundUpToPowerOfTwo(requestSizeInBytes > minimumPoolBlockSizeInBytes ? requestSizeInBytes : minimumPoolBlockSizeInBytes);

	BufferObjectCreateOptions uploadBufferCreateOptions = {};
	uploadBufferCreateOptions.sizeInBytes = poolBlockSizeInBytes;
	uploadBufferCreateOptions.memoryType = BufferObjectMemoryType::uploadHeap;
	unique_pointer<BufferResourceObject> uploadBufferObject = renderBackend.createBufferObject(uploadBufferCreateOptions);
	if (uploadBufferObject == nullptr)
	{
		error << "[GPUUploader][Error] reason=upload_pool_block_create_failed sizeInBytes="
			  << poolBlockSizeInBytes << lineBreak;
		return false;
	}

	ID3D12Resource* dx12UploadBuffer = static_cast<ID3D12Resource*>(uploadBufferObject->getNativeResource());
	if (dx12UploadBuffer == nullptr)
	{
		error << "[GPUUploader][Error] reason=upload_pool_block_native_resource_missing sizeInBytes="
			  << poolBlockSizeInBytes << lineBreak;
		return false;
	}

	void* mappedMemory = nullptr;
	D3D12_RANGE readRange = {};
	readRange.Begin = 0;
	readRange.End = 0;
	if (FAILED(dx12UploadBuffer->Map(0, &readRange, &mappedMemory)) || mappedMemory == nullptr)
	{
		error << "[GPUUploader][Error] reason=upload_pool_block_map_failed sizeInBytes="
			  << poolBlockSizeInBytes << lineBreak;
		return false;
	}

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
		ID3D12Resource* dx12Resource = static_cast<ID3D12Resource*>(poolBlock.bufferObject->getNativeResource());
		if (dx12Resource != nullptr && poolBlock.mappedMemory != nullptr)
		{
			dx12Resource->Unmap(0, nullptr);
		}
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
		uploadBufferPoolBlocks[blockIndex].usedInBytes = 0;
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
