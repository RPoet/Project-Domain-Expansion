#include "Render/HeapSuballocator.h"

#include "Engine/Common/Math/ScalarMath.h"
#include "Render/Backends/RenderBackend.h"

bool HeapSuballocator::initialize(RenderBackend& renderBackend, const HeapObjectCreateOptions& createOptions)
{
	clear();

	if (createOptions.sizeInBytes == 0)
	{
		return false;
	}

	unique_pointer<HeapObject> createdHeapObject = renderBackend.createHeapObject(createOptions);
	if (createdHeapObject == nullptr)
	{
		return false;
	}

	heapObject = moveValue(createdHeapObject);
	capacityInBytes = createOptions.sizeInBytes;
	freeBlocks.push_back(FreeBlock{ 0, capacityInBytes });
	return true;
}

void HeapSuballocator::clear()
{
	heapObject.reset();
	freeBlocks.clear();
	allocatedBlocks.clear();
	capacityInBytes = 0;
}

// TO DO : Refactor this function
HeapSuballocation HeapSuballocator::allocate(const uint64 sizeInBytes, const uint64 alignmentInBytes)
{
	if (!isInitialized() || sizeInBytes == 0)
	{
		return {};
	}

	for (uint32 blockIndex = 0; blockIndex < static_cast<uint32>(freeBlocks.size()); ++blockIndex)
	{
		FreeBlock& freeBlock = freeBlocks[blockIndex];
		const uint64 alignedOffset = alignUp(freeBlock.offsetInBytes, alignmentInBytes);
		const uint64 alignmentPadding = alignedOffset - freeBlock.offsetInBytes;
		if (alignmentPadding > freeBlock.sizeInBytes)
		{
			continue;
		}

		const uint64 availableSizeInBytes = freeBlock.sizeInBytes - alignmentPadding;
		if (availableSizeInBytes < sizeInBytes)
		{
			continue;
		}

		const uint64 allocatedEndOffset = alignedOffset + sizeInBytes;
		const uint64 freeBlockEndOffset = freeBlock.offsetInBytes + freeBlock.sizeInBytes;
		if (alignmentPadding != 0 && allocatedEndOffset < freeBlockEndOffset)
		{
			const FreeBlock tailBlock =
			{
				.offsetInBytes = allocatedEndOffset,
				.sizeInBytes = freeBlockEndOffset - allocatedEndOffset,
			};
			freeBlock.sizeInBytes = alignmentPadding;
			freeBlocks.insert(freeBlocks.begin() + blockIndex + 1, tailBlock);
		}
		else if (alignmentPadding != 0)
		{
			freeBlock.sizeInBytes = alignmentPadding;
		}
		else if (allocatedEndOffset < freeBlockEndOffset)
		{
			freeBlock.offsetInBytes = allocatedEndOffset;
			freeBlock.sizeInBytes = freeBlockEndOffset - allocatedEndOffset;
		}
		else
		{
			freeBlocks.erase(freeBlocks.begin() + blockIndex);
		}

		const FreeBlock allocatedBlock
		{
			.offsetInBytes = alignedOffset,
			.sizeInBytes = sizeInBytes,
		};
		const auto allocatedBlockInsertIterator = std::lower_bound(
			allocatedBlocks.begin(),
			allocatedBlocks.end(),
			allocatedBlock.offsetInBytes,
			[](const FreeBlock& block, const uint64 searchedOffset)
			{
				return block.offsetInBytes < searchedOffset;
			});
		allocatedBlocks.insert(allocatedBlockInsertIterator, allocatedBlock);

		return HeapSuballocation
		{
			.offsetInBytes = alignedOffset,
			.sizeInBytes = sizeInBytes,
		};
	}

	return {};
}

// TO DO : Refactor this function
void HeapSuballocator::release(const uint64 offsetInBytes)
{
	if (!isInitialized())
	{
		return;
	}

	const auto allocationIterator = std::lower_bound(
		allocatedBlocks.begin(),
		allocatedBlocks.end(),
		offsetInBytes,
		[](const FreeBlock& block, const uint64 searchedOffset)
		{
			return block.offsetInBytes < searchedOffset;
		});
	assert(allocationIterator != allocatedBlocks.end() && allocationIterator->offsetInBytes == offsetInBytes && "");

	const FreeBlock releasedBlock = *allocationIterator;
	allocatedBlocks.erase(allocationIterator);

	const auto insertIterator = std::lower_bound(
		freeBlocks.begin(),
		freeBlocks.end(),
		releasedBlock.offsetInBytes,
		[](const FreeBlock& block, const uint64 searchedOffset)
		{
			return block.offsetInBytes < searchedOffset;
		});
	const uint32 insertIndex = static_cast<uint32>(insertIterator - freeBlocks.begin());
	const bool canMergePrevious = insertIndex > 0
		&& freeBlocks[insertIndex - 1].offsetInBytes + freeBlocks[insertIndex - 1].sizeInBytes == releasedBlock.offsetInBytes;
	const bool canMergeNext = insertIndex < static_cast<uint32>(freeBlocks.size())
		&& releasedBlock.offsetInBytes + releasedBlock.sizeInBytes == freeBlocks[insertIndex].offsetInBytes;

	if (canMergePrevious)
	{
		FreeBlock& previousBlock = freeBlocks[insertIndex - 1];
		previousBlock.sizeInBytes += releasedBlock.sizeInBytes;
		if (canMergeNext)
		{
			previousBlock.sizeInBytes += freeBlocks[insertIndex].sizeInBytes;
			freeBlocks.erase(freeBlocks.begin() + insertIndex);
		}
		return;
	}

	if (canMergeNext)
	{
		FreeBlock& nextBlock = freeBlocks[insertIndex];
		nextBlock.offsetInBytes = releasedBlock.offsetInBytes;
		nextBlock.sizeInBytes += releasedBlock.sizeInBytes;
		return;
	}

	freeBlocks.insert(insertIterator, releasedBlock);
}

bool HeapSuballocator::isInitialized() const
{
	return heapObject != nullptr && capacityInBytes != 0;
}

uint64 HeapSuballocator::getCapacityInBytes() const
{
	return capacityInBytes;
}

HeapObject* HeapSuballocator::getHeapObject()
{
	return heapObject.get();
}

const HeapObject* HeapSuballocator::getHeapObject() const
{
	return heapObject.get();
}
