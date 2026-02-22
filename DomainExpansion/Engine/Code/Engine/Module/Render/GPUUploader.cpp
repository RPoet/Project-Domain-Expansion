#include "Engine/Module/Render/GPUUploader.h"

#include "Render/CommandList.h"
#include "Render/Backends/RenderBackend.h"

bool GPUUploader::init(Framework& framework)
{
	unused(framework);
	uploaderMode = GPUUploaderMode::staging;
	currentFenceValue = 0;
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
	currentFenceValue = 0;
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

	if (!reserveUploadSpace(uploadRequestOptions.sourceDataSizeInBytes))
	{
		error << "[GPUUploader][Error] reason=upload_pool_reserve_failed sourceDataSizeInBytes="
			  << uploadRequestOptions.sourceDataSizeInBytes << lineBreak;
		return nullptr;
	}

	BufferObjectCreateOptions destinationBufferCreateOptions = createOptions;
	destinationBufferCreateOptions.memoryType = BufferObjectMemoryType::defaultHeap;
	unique_pointer<BufferResourceObject> createdBufferObject = renderBackend.createBufferObject(destinationBufferCreateOptions);
	if (createdBufferObject == nullptr)
	{
		error << "[GPUUploader][Error] reason=backend_create_buffer_failed sizeInBytes="
			  << createOptions.sizeInBytes << lineBreak;
	}

	// TO DO : Queue upload request and consume it via uploadQueuedBuffers(commandList).
	return createdBufferObject;
}

void GPUUploader::setFenceValue(const uint64 fenceValue)
{
	currentFenceValue = fenceValue;
}

void GPUUploader::uploadQueuedBuffers(CommandList& commandList)
{
	unused(commandList);
	// TO DO : Record queued buffer copies through CommandList::copyBuffer.
}

bool GPUUploader::reserveUploadSpace(const uint64 requestSizeInBytes)
{
	if (requestSizeInBytes == 0)
	{
		return true;
	}

	for (uint32 blockIndex = 0; blockIndex < static_cast<uint32>(uploadBufferPoolBlocks.size()); ++blockIndex)
	{
		UploadBufferPoolBlock& block = uploadBufferPoolBlocks[blockIndex];
		if (block.capacityInBytes < block.usedInBytes
			|| block.capacityInBytes - block.usedInBytes < requestSizeInBytes)
		{
			continue;
		}

		block.usedInBytes += requestSizeInBytes;
		block.lastUsedFenceValue = currentFenceValue;
		return true;
	}

	const uint64 targetBlockSize = roundUpToPowerOfTwo(requestSizeInBytes > minimumPoolBlockSizeInBytes ? requestSizeInBytes : minimumPoolBlockSizeInBytes);
	UploadBufferPoolBlock newBlock = {};
	newBlock.capacityInBytes = targetBlockSize;
	newBlock.usedInBytes = requestSizeInBytes;
	newBlock.lastUsedFenceValue = currentFenceValue;
	uploadBufferPoolBlocks.push_back(newBlock);
	return true;
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
		if (block.lastUsedFenceValue + idleReleaseFenceThreshold >= currentFenceValue)
		{
			continue;
		}

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
	uploadBufferPoolBlocks.clear();
}
