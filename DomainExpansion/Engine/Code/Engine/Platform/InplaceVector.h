#pragma once

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>

template <typename ElementType, std::size_t InplaceCapacity>
class InplaceVector
{
	static_assert(InplaceCapacity > 0, "InplaceVector requires InplaceCapacity > 0.");

public:
	using value_type = ElementType;
	using size_type = std::size_t;

	InplaceVector() = default;

	InplaceVector(std::initializer_list<ElementType> initialValues)
	{
		reserve(initialValues.size());
		for (const ElementType& initialValue : initialValues)
		{
			emplace_back(initialValue);
		}
	}

	InplaceVector(const InplaceVector& other)
	{
		copyFrom(other);
	}

	InplaceVector& operator=(const InplaceVector& other)
	{
		if (this == &other)
		{
			return *this;
		}

		clear();
		releaseHeapStorage();
		copyFrom(other);
		return *this;
	}

	InplaceVector(InplaceVector&& other) noexcept
	{
		moveFrom(static_cast<InplaceVector&&>(other));
	}

	InplaceVector& operator=(InplaceVector&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}

		clear();
		releaseHeapStorage();
		moveFrom(static_cast<InplaceVector&&>(other));
		return *this;
	}

	~InplaceVector()
	{
		clear();
		releaseHeapStorage();
	}

	size_type size() const
	{
		return elementCount;
	}

	size_type capacity() const
	{
		return isUsingHeapStorage() ? heapCapacity : InplaceCapacity;
	}

	bool empty() const
	{
		return elementCount == 0;
	}

	ElementType* data()
	{
		return isUsingHeapStorage() ? heapStorage : getInlineStorage();
	}

	const ElementType* data() const
	{
		return isUsingHeapStorage() ? heapStorage : getInlineStorage();
	}

	ElementType* begin()
	{
		return data();
	}

	const ElementType* begin() const
	{
		return data();
	}

	const ElementType* cbegin() const
	{
		return data();
	}

	ElementType* end()
	{
		return data() + elementCount;
	}

	const ElementType* end() const
	{
		return data() + elementCount;
	}

	const ElementType* cend() const
	{
		return data() + elementCount;
	}

	ElementType& operator[](const size_type index)
	{
		return data()[index];
	}

	const ElementType& operator[](const size_type index) const
	{
		return data()[index];
	}

	void reserve(const size_type requestedCapacity)
	{
		if (requestedCapacity <= capacity())
		{
			return;
		}

		reallocate(requestedCapacity);
	}

	template <typename... ArgumentTypes>
	ElementType& emplace_back(ArgumentTypes&&... arguments)
	{
		ensureCapacityForAppend();
		ElementType* destination = data() + elementCount;
		std::construct_at(destination, std::forward<ArgumentTypes>(arguments)...);
		++elementCount;
		return *destination;
	}

	void push_back(const ElementType& value)
	{
		emplace_back(value);
	}

	void push_back(ElementType&& value)
	{
		emplace_back(std::move(value));
	}

	void pop_back()
	{
		if (elementCount == 0)
		{
			return;
		}

		--elementCount;
		std::destroy_at(data() + elementCount);
	}

	void clear()
	{
		ElementType* currentData = data();
		for (size_type elementIndex = 0; elementIndex < elementCount; ++elementIndex)
		{
			std::destroy_at(currentData + elementIndex);
		}

		elementCount = 0;
	}

private:
	using AllocatorType = std::allocator<ElementType>;
	using AllocatorTraits = std::allocator_traits<AllocatorType>;
	using InlineStorageType = std::aligned_storage_t<sizeof(ElementType), alignof(ElementType)>;

	void ensureCapacityForAppend()
	{
		if (elementCount < capacity())
		{
			return;
		}

		size_type nextCapacity = capacity() * 2;
		if (nextCapacity <= capacity())
		{
			nextCapacity = capacity() + 1;
		}

		reallocate(nextCapacity);
	}

	void reallocate(const size_type requestedCapacity)
	{
		if (requestedCapacity <= capacity())
		{
			return;
		}

		ElementType* newStorage = AllocatorTraits::allocate(allocator, requestedCapacity);
		ElementType* currentData = data();
		for (size_type elementIndex = 0; elementIndex < elementCount; ++elementIndex)
		{
			std::construct_at(newStorage + elementIndex, std::move(currentData[elementIndex]));
			std::destroy_at(currentData + elementIndex);
		}

		if (isUsingHeapStorage())
		{
			AllocatorTraits::deallocate(allocator, heapStorage, heapCapacity);
		}

		heapStorage = newStorage;
		heapCapacity = requestedCapacity;
	}

	void releaseHeapStorage()
	{
		if (!isUsingHeapStorage())
		{
			return;
		}

		AllocatorTraits::deallocate(allocator, heapStorage, heapCapacity);
		heapStorage = nullptr;
		heapCapacity = 0;
	}

	void copyFrom(const InplaceVector& other)
	{
		reserve(other.size());
		for (size_type elementIndex = 0; elementIndex < other.size(); ++elementIndex)
		{
			emplace_back(other[elementIndex]);
		}
	}

	void moveFrom(InplaceVector&& other)
	{
		if (other.isUsingHeapStorage())
		{
			heapStorage = other.heapStorage;
			heapCapacity = other.heapCapacity;
			elementCount = other.elementCount;
			other.heapStorage = nullptr;
			other.heapCapacity = 0;
			other.elementCount = 0;
			return;
		}

		ElementType* inlineStoragePointer = getInlineStorage();
		ElementType* otherStoragePointer = other.getInlineStorage();
		for (size_type elementIndex = 0; elementIndex < other.elementCount; ++elementIndex)
		{
			std::construct_at(inlineStoragePointer + elementIndex, std::move(otherStoragePointer[elementIndex]));
			std::destroy_at(otherStoragePointer + elementIndex);
		}

		elementCount = other.elementCount;
		other.elementCount = 0;
	}

	bool isUsingHeapStorage() const
	{
		return heapStorage != nullptr;
	}

	ElementType* getInlineStorage()
	{
		return reinterpret_cast<ElementType*>(inlineStorage);
	}

	const ElementType* getInlineStorage() const
	{
		return reinterpret_cast<const ElementType*>(inlineStorage);
	}

	AllocatorType allocator = {};
	InlineStorageType inlineStorage[InplaceCapacity] = {};
	ElementType* heapStorage = nullptr;
	size_type heapCapacity = 0;
	size_type elementCount = 0;
};
