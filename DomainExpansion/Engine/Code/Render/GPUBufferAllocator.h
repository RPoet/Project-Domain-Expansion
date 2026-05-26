#pragma once

#include "Render/HeapSuballocator.h"
#include "Render/Backends/ResourceObject.h"

class RenderBackend;

struct GPUBufferAllocatorCreateOptions
{
	uint64 heapSizeInBytes = 64ull * 1024ull * 1024ull;
	uint64 heapAlignmentInBytes = 0;
	HeapObjectFlag heapFlags = HeapObjectFlag::allowOnlyBuffers;
};

class GPUBufferAllocator final : private NonCopiable
{
public:
	GPUBufferAllocator() = default;
	~GPUBufferAllocator() = default;

	void initialize(const GPUBufferAllocatorCreateOptions& createOptions);
	void clear();
	unique_pointer<BufferResourceObject> allocate(RenderBackend& renderBackend, const BufferObjectCreateOptions& bufferCreateOptions);
	void release(unique_pointer<BufferResourceObject>& bufferObject);
	HeapObject* getHeapObject(uint64 gpuVirtualAddress);
	const HeapObject* getHeapObject(uint64 gpuVirtualAddress) const;

private:
	HeapSuballocator* createHeap(RenderBackend& renderBackend, uint64 minimumSizeInBytes, uint64 alignmentInBytes);
	HeapSuballocator* getHeap(uint8 heapId);
	const HeapSuballocator* getHeap(uint8 heapId) const;

	GPUBufferAllocatorCreateOptions allocatorCreateOptions = {};
	ResourceAllocationInfo cachedBufferAllocationInfo = {};
	bool bufferAllocationInfoCached = false;
	vector<unique_pointer<HeapSuballocator>> heaps;
};
