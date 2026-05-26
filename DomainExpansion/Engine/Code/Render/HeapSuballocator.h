#pragma once

#include "Engine/Common/NonCopiable.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Render/Backends/HeapObject.h"

class RenderBackend;

struct HeapSuballocation
{
	uint64 offsetInBytes = 0;
	uint64 sizeInBytes = 0;
};

class HeapSuballocator final : private NonCopiable
{
public:
	HeapSuballocator() = default;
	~HeapSuballocator() = default;

	bool initialize(RenderBackend& renderBackend, const HeapObjectCreateOptions& createOptions);
	void clear();
	HeapSuballocation allocate(uint64 sizeInBytes, uint64 alignmentInBytes = 1);
	void release(uint64 offsetInBytes);
	bool isInitialized() const;
	uint64 getCapacityInBytes() const;
	HeapObject* getHeapObject();
	const HeapObject* getHeapObject() const;

private:
	struct FreeBlock
	{
		uint64 offsetInBytes = 0;
		uint64 sizeInBytes = 0;
	};

	unique_pointer<HeapObject> heapObject = nullptr;
	vector<FreeBlock> freeBlocks;
	vector<FreeBlock> allocatedBlocks;
	uint64 capacityInBytes = 0;
};
