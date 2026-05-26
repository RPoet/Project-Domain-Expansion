#pragma once

#include "Engine/Platform/PlatformDefine.h"

inline constexpr uint32 heapIdBitCount = 8;
inline constexpr uint32 heapOffsetBitCount = 64 - heapIdBitCount;
inline constexpr uint64 heapOffsetMask = (1ull << heapOffsetBitCount) - 1ull;
inline constexpr uint64 invalidGpuVirtualAddress = ~0ull;

enum class HeapObjectMemoryType : uint32
{
	defaultHeap = 0,
	uploadHeap = 1,
	readbackHeap = 2,
};

enum class HeapObjectFlag : uint32
{
	none = 0,
	allowOnlyBuffers = 1,
	allowOnlyNonRtDsTextures = 2,
	allowOnlyRtDsTextures = 4,
	// Check whether It is possible to contain resource regardless its types.
	// ...
};

struct HeapObjectCreateOptions
{
	uint64 sizeInBytes = 0;
	uint64 alignment = 0;
	HeapObjectMemoryType memoryType = HeapObjectMemoryType::defaultHeap;
	HeapObjectFlag flags = HeapObjectFlag::none;
};

class HeapObject
{
public:
	virtual ~HeapObject() = default;

	virtual const void* getNativeHeap() const { return nullptr; }
	virtual void* getNativeHeap() { return nullptr; }
};
