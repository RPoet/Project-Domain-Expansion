#include <malloc.h>

#include "Engine/Common/FileStream.h"

void* operator new(const decltype(sizeof(0)) allocationSize)
{
	void* allocatedMemory = malloc(allocationSize);
	if (allocatedMemory == nullptr)
	{
		throw bad_alloc();
	}

	return allocatedMemory;
}

void* operator new[](const decltype(sizeof(0)) allocationSize)
{
	void* allocatedMemory = malloc(allocationSize);
	if (allocatedMemory == nullptr)
	{
		throw bad_alloc();
	}

	return allocatedMemory;
}

void* operator new(const decltype(sizeof(0)) allocationSize, const nothrow_t&) noexcept
{
	try
	{
		void* allocatedMemory = malloc(allocationSize);
		return allocatedMemory;
	}
	catch (...)
	{
		return nullptr;
	}
}

void* operator new[](const decltype(sizeof(0)) allocationSize, const nothrow_t&) noexcept
{
	try
	{
		void* allocatedMemory = malloc(allocationSize);
		return allocatedMemory;
	}
	catch (...)
	{
		return nullptr;
	}
}

void* operator new(const decltype(sizeof(0)) allocationSize, const align_val_t alignment)
{
	void* allocatedMemory = _aligned_malloc(allocationSize, static_cast<decltype(sizeof(0))>(alignment));
	if (allocatedMemory == nullptr)
	{
		throw bad_alloc();
	}

	return allocatedMemory;
}

void* operator new[](const decltype(sizeof(0)) allocationSize, const align_val_t alignment)
{
	void* allocatedMemory = _aligned_malloc(allocationSize, static_cast<decltype(sizeof(0))>(alignment));
	if (allocatedMemory == nullptr)
	{
		throw bad_alloc();
	}

	return allocatedMemory;
}

void* operator new(const decltype(sizeof(0)) allocationSize, const align_val_t alignment, const nothrow_t&) noexcept
{
	try
	{
		void* allocatedMemory = _aligned_malloc(allocationSize, static_cast<decltype(sizeof(0))>(alignment));
		return allocatedMemory;
	}
	catch (...)
	{
		return nullptr;
	}
}

void* operator new[](const decltype(sizeof(0)) allocationSize, const align_val_t alignment, const nothrow_t&) noexcept
{
	try
	{
		void* allocatedMemory = _aligned_malloc(allocationSize, static_cast<decltype(sizeof(0))>(alignment));
		return allocatedMemory;
	}
	catch (...)
	{
		return nullptr;
	}
}

void operator delete(void* allocatedMemory) noexcept
{
	free(allocatedMemory);
}

void operator delete[](void* allocatedMemory) noexcept
{
	free(allocatedMemory);
}

void operator delete(void* allocatedMemory, const decltype(sizeof(0))) noexcept
{
	free(allocatedMemory);
}

void operator delete[](void* allocatedMemory, const decltype(sizeof(0))) noexcept
{
	free(allocatedMemory);
}

void operator delete(void* allocatedMemory, const nothrow_t&) noexcept
{
	free(allocatedMemory);
}

void operator delete[](void* allocatedMemory, const nothrow_t&) noexcept
{
	free(allocatedMemory);
}

void operator delete(void* allocatedMemory, const align_val_t) noexcept
{
	_aligned_free(allocatedMemory);
}

void operator delete[](void* allocatedMemory, const align_val_t) noexcept
{
	_aligned_free(allocatedMemory);
}

void operator delete(void* allocatedMemory, const decltype(sizeof(0)), const align_val_t) noexcept
{
	_aligned_free(allocatedMemory);
}

void operator delete[](void* allocatedMemory, const decltype(sizeof(0)), const align_val_t) noexcept
{
	_aligned_free(allocatedMemory);
}

void operator delete(void* allocatedMemory, const align_val_t, const nothrow_t&) noexcept
{
	_aligned_free(allocatedMemory);
}

void operator delete[](void* allocatedMemory, const align_val_t, const nothrow_t&) noexcept
{
	_aligned_free(allocatedMemory);
}
