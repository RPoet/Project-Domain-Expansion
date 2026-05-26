#include "Render/GPUBufferAllocator.h"

#include "Engine/Common/Math/ScalarMath.h"
#include "Render/Backends/RenderBackend.h"

namespace
{
uint64 encodeGpuVirtualAddress(const uint8 heapId, const uint64 heapOffsetInBytes)
{
	return (static_cast<uint64>(heapId) << heapOffsetBitCount) | (heapOffsetInBytes & heapOffsetMask);
}

uint8 getHeapId(const uint64 gpuVirtualAddress)
{
	return static_cast<uint8>(gpuVirtualAddress >> heapOffsetBitCount);
}

uint64 getHeapOffsetInBytes(const uint64 gpuVirtualAddress)
{
	return gpuVirtualAddress & heapOffsetMask;
}
}

void GPUBufferAllocator::initialize(const GPUBufferAllocatorCreateOptions& createOptions)
{
	clear();
	allocatorCreateOptions = createOptions;
}

void GPUBufferAllocator::clear()
{
	cachedBufferAllocationInfo = {};
	bufferAllocationInfoCached = false;
	heaps.clear();
}

unique_pointer<BufferResourceObject> GPUBufferAllocator::allocate(RenderBackend& renderBackend, const BufferObjectCreateOptions& bufferCreateOptions)
{
	if (!bufferAllocationInfoCached)
	{
		BufferObjectCreateOptions allocationInfoCreateOptions = bufferCreateOptions;
		allocationInfoCreateOptions.sizeInBytes = 1;
		cachedBufferAllocationInfo = renderBackend.getBufferObjectAllocationInfo(allocationInfoCreateOptions);
		bufferAllocationInfoCached = true;
		assert(cachedBufferAllocationInfo.sizeInBytes != 0 || cachedBufferAllocationInfo.alignmentInBytes != 0);
	}
	assert(bufferCreateOptions.sizeInBytes != 0);

	uint64 allocationSizeInBytes = alignUp(bufferCreateOptions.sizeInBytes, cachedBufferAllocationInfo.alignmentInBytes);
	BufferObjectCreateOptions placedBufferCreateOptions =
	{
		.placedResource = 1,
		.reservedResource = 0,
		.sizeInBytes = bufferCreateOptions.sizeInBytes,
		.memoryType = BufferObjectMemoryType::defaultHeap,
		.initialState = bufferCreateOptions.initialState == ResourceState::unknown ? ResourceState::common : bufferCreateOptions.initialState,
	};

	for (uint32 heapIndex = 0; heapIndex < static_cast<uint32>(heaps.size()); ++heapIndex)
	{
		HeapSuballocator* heapSuballocator = heaps[heapIndex].get();
		if (heapSuballocator == nullptr)
		{
			continue;
		}

		const HeapSuballocation heapSuballocation = heapSuballocator->allocate(allocationSizeInBytes, cachedBufferAllocationInfo.alignmentInBytes);
		if (heapSuballocation.sizeInBytes == 0)
		{
			continue;
		}

		placedBufferCreateOptions.heapObject = heapSuballocator->getHeapObject();
		placedBufferCreateOptions.gpuVirtualAddress = encodeGpuVirtualAddress(static_cast<uint8>(heapIndex), heapSuballocation.offsetInBytes);
		unique_pointer<BufferResourceObject> createdBufferObject = renderBackend.createBufferObject(placedBufferCreateOptions);

		return moveValue(createdBufferObject);
	}

	HeapSuballocator* createdHeap = createHeap(renderBackend, allocationSizeInBytes, cachedBufferAllocationInfo.alignmentInBytes);
	const uint8 heapIndex = static_cast<uint8>(heaps.size() - 1);
	const HeapSuballocation heapSuballocation = createdHeap->allocate(allocationSizeInBytes, cachedBufferAllocationInfo.alignmentInBytes);
	placedBufferCreateOptions.heapObject = createdHeap->getHeapObject();
	placedBufferCreateOptions.gpuVirtualAddress = encodeGpuVirtualAddress(static_cast<uint8>(heapIndex), heapSuballocation.offsetInBytes);

	unique_pointer<BufferResourceObject> createdBufferObject = renderBackend.createBufferObject(placedBufferCreateOptions);
	return moveValue(createdBufferObject);
}

void GPUBufferAllocator::release(unique_pointer<BufferResourceObject>& bufferObject)
{
	if (bufferObject == nullptr)
	{
		return;
	}

	const uint64 gpuVirtualAddress = bufferObject->getOptions().gpuVirtualAddress;
	if (gpuVirtualAddress == invalidGpuVirtualAddress)
	{
		bufferObject.reset();
		return;
	}

	bufferObject.reset();

	HeapSuballocator* heapSuballocator = getHeap(getHeapId(gpuVirtualAddress));
	if (heapSuballocator == nullptr)
	{
		return;
	}

	heapSuballocator->release(getHeapOffsetInBytes(gpuVirtualAddress));
}

HeapObject* GPUBufferAllocator::getHeapObject(const uint64 gpuVirtualAddress)
{
	if (gpuVirtualAddress == invalidGpuVirtualAddress)
	{
		return nullptr;
	}

	HeapSuballocator* heapSuballocator = getHeap(getHeapId(gpuVirtualAddress));
	if (heapSuballocator == nullptr)
	{
		return nullptr;
	}

	return heapSuballocator->getHeapObject();
}

const HeapObject* GPUBufferAllocator::getHeapObject(const uint64 gpuVirtualAddress) const
{
	if (gpuVirtualAddress == invalidGpuVirtualAddress)
	{
		return nullptr;
	}

	const HeapSuballocator* heapSuballocator = getHeap(getHeapId(gpuVirtualAddress));
	if (heapSuballocator == nullptr)
	{
		return nullptr;
	}

	return heapSuballocator->getHeapObject();
}

HeapSuballocator* GPUBufferAllocator::createHeap(
	RenderBackend& renderBackend,
	const uint64 minimumSizeInBytes,
	const uint64 alignmentInBytes)
{
	uint64 heapSizeInBytes = allocatorCreateOptions.heapSizeInBytes;
	if (heapSizeInBytes < minimumSizeInBytes)
	{
		heapSizeInBytes = minimumSizeInBytes;
	}
	if (alignmentInBytes > 1)
	{
		const uint64 alignmentRemainder = heapSizeInBytes % alignmentInBytes;
		if (alignmentRemainder != 0)
		{
			const uint64 alignmentPaddingInBytes = alignmentInBytes - alignmentRemainder;
			if (alignmentPaddingInBytes > heapOffsetMask || heapSizeInBytes > heapOffsetMask - alignmentPaddingInBytes)
			{
				return nullptr;
			}

			heapSizeInBytes += alignmentPaddingInBytes;
		}
	}
	if (heapSizeInBytes > heapOffsetMask)
	{
		return nullptr;
	}

	unique_pointer<HeapSuballocator> heapSuballocator(new HeapSuballocator());
	const HeapObjectCreateOptions heapCreateOptions
	{
		.sizeInBytes = heapSizeInBytes,
		.alignment = allocatorCreateOptions.heapAlignmentInBytes,
		.memoryType = HeapObjectMemoryType::defaultHeap,
		.flags = allocatorCreateOptions.heapFlags,
	};
	if (!heapSuballocator->initialize(renderBackend, heapCreateOptions))
	{
		return nullptr;
	}

	heaps.push_back(moveValue(heapSuballocator));
	return heaps[static_cast<uint32>(heaps.size() - 1)].get();
}

HeapSuballocator* GPUBufferAllocator::getHeap(const uint8 heapId)
{
	if (static_cast<uint32>(heapId) >= static_cast<uint32>(heaps.size()))
	{
		return nullptr;
	}

	return heaps[static_cast<uint32>(heapId)].get();
}

const HeapSuballocator* GPUBufferAllocator::getHeap(const uint8 heapId) const
{
	if (static_cast<uint32>(heapId) >= static_cast<uint32>(heaps.size()))
	{
		return nullptr;
	}

	return heaps[static_cast<uint32>(heapId)].get();
}
