#pragma once

#include <cstddef>
#include <cstdlib>

#include "Engine/Common/FileStream.h"

template <typename value_type>
class vector
{
public:
	using size_type = decltype(sizeof(0));
	using iterator = value_type*;
	using const_iterator = const value_type*;

	vector() = default;

	explicit vector(const size_type initialCount)
	{
		resize(initialCount);
	}

	vector(const size_type initialCount, const value_type& fillValue)
	{
		resize(initialCount, fillValue);
	}

	vector(initializer_list<value_type> initialValues)
	{
		reserve(initialValues.size());
		for (const value_type& initialValue : initialValues)
		{
			emplace_back(initialValue);
		}
	}

	vector(const vector& other)
	{
		copyFrom(other);
	}

	vector& operator=(const vector& other)
	{
		if (this == &other)
		{
			return *this;
		}

		clear();
		releaseStorage();
		copyFrom(other);
		return *this;
	}

	vector(vector&& other) noexcept
	{
		moveFrom(static_cast<vector&&>(other));
	}

	vector& operator=(vector&& other) noexcept
	{
		if (this == &other)
		{
			return *this;
		}

		clear();
		releaseStorage();
		moveFrom(static_cast<vector&&>(other));
		return *this;
	}

	~vector()
	{
		clear();
		releaseStorage();
	}

	size_type size() const
	{
		return elementCount;
	}

	size_type capacity() const
	{
		return storageCapacity;
	}

	bool empty() const
	{
		return elementCount == 0;
	}

	value_type* data()
	{
		return storage != nullptr ? storage : getEmptyStorage();
	}

	const value_type* data() const
	{
		return storage != nullptr ? storage : getEmptyStorage();
	}

	iterator begin()
	{
		return data();
	}

	const_iterator begin() const
	{
		return data();
	}

	const_iterator cbegin() const
	{
		return data();
	}

	iterator end()
	{
		return data() + elementCount;
	}

	const_iterator end() const
	{
		return data() + elementCount;
	}

	const_iterator cend() const
	{
		return data() + elementCount;
	}

	value_type& operator[](const size_type index)
	{
		assert(index < elementCount && "[vector][Assert] reason=index_out_of_range");
		return storage[index];
	}

	const value_type& operator[](const size_type index) const
	{
		assert(index < elementCount && "[vector][Assert] reason=index_out_of_range");
		return storage[index];
	}

	value_type& front()
	{
		assert(elementCount > 0 && "[vector][Assert] reason=front_on_empty");
		return storage[0];
	}

	const value_type& front() const
	{
		assert(elementCount > 0 && "[vector][Assert] reason=front_on_empty");
		return storage[0];
	}

	value_type& back()
	{
		assert(elementCount > 0 && "[vector][Assert] reason=back_on_empty");
		return storage[elementCount - 1];
	}

	const value_type& back() const
	{
		assert(elementCount > 0 && "[vector][Assert] reason=back_on_empty");
		return storage[elementCount - 1];
	}

	void reserve(const size_type requestedCapacity)
	{
		if (requestedCapacity <= storageCapacity)
		{
			return;
		}

		reallocate(requestedCapacity);
	}

	void resize(const size_type requestedSize)
	{
		if (requestedSize < elementCount)
		{
			if constexpr (!std::is_trivially_destructible_v<value_type>)
			{
				for (size_type elementIndex = requestedSize; elementIndex < elementCount; ++elementIndex)
				{
					destroyAt(storage + elementIndex);
				}
			}

			elementCount = requestedSize;
			return;
		}

		if (requestedSize > elementCount)
		{
			reserve(requestedSize);
			for (size_type elementIndex = elementCount; elementIndex < requestedSize; ++elementIndex)
			{
				constructAt(storage + elementIndex);
			}

			elementCount = requestedSize;
		}
	}

	void resize(const size_type requestedSize, const value_type& fillValue)
	{
		if (requestedSize < elementCount)
		{
			if constexpr (!std::is_trivially_destructible_v<value_type>)
			{
				for (size_type elementIndex = requestedSize; elementIndex < elementCount; ++elementIndex)
				{
					destroyAt(storage + elementIndex);
				}
			}

			elementCount = requestedSize;
			return;
		}

		if (requestedSize > elementCount)
		{
			reserve(requestedSize);
			for (size_type elementIndex = elementCount; elementIndex < requestedSize; ++elementIndex)
			{
				constructAt(storage + elementIndex, fillValue);
			}

			elementCount = requestedSize;
		}
	}

	template <typename overwrite_value_type = value_type>
	enable_if<is_trivially_copyable<overwrite_value_type>, void>
	resizeUninitialized(const size_type requestedSize)
	{
		assert(elementCount == 0 && "[vector][Assert] reason=overwrite_resize_requires_cleared_vector");
		if (requestedSize > storageCapacity)
		{
			reallocate(requestedSize);
		}

		elementCount = requestedSize;
	}

	template <typename... argument_types>
	value_type& emplace_back(argument_types&&... arguments)
	{
		ensureAppendCapacity();
		value_type* destination = storage + elementCount;
		constructAt(destination, forwardValue<argument_types>(arguments)...);
		++elementCount;
		return *destination;
	}

	void push_back(const value_type& value)
	{
		emplace_back(value);
	}

	void push_back(value_type&& value)
	{
		emplace_back(moveValue(value));
	}

	template <typename... argument_types>
	iterator emplace(const_iterator position, argument_types&&... arguments)
	{
		assert(position >= cbegin() && position <= cend() && "[vector][Assert] reason=insert_position_out_of_range");
		const size_type insertIndex = static_cast<size_type>(position - cbegin());
		if (insertIndex >= elementCount)
		{
			emplace_back(forwardValue<argument_types>(arguments)...);
			return begin() + (elementCount - 1);
		}

		ensureAppendCapacity();
		constructAt(storage + elementCount, moveValue(storage[elementCount - 1]));
		for (size_type moveIndex = elementCount - 1; moveIndex > insertIndex; --moveIndex)
		{
			storage[moveIndex] = moveValue(storage[moveIndex - 1]);
		}

		storage[insertIndex] = value_type(forwardValue<argument_types>(arguments)...);
		++elementCount;
		return begin() + insertIndex;
	}

	iterator insert(const_iterator position, const value_type& value)
	{
		return emplace(position, value);
	}

	iterator insert(const_iterator position, value_type&& value)
	{
		return emplace(position, moveValue(value));
	}

	iterator insert(const_iterator position, const_iterator first, const_iterator last)
	{
		assert(position >= cbegin() && position <= cend() && "[vector][Assert] reason=insert_position_out_of_range");
		const size_type insertIndex = static_cast<size_type>(position - cbegin());
		size_type nextInsertIndex = insertIndex;
		for (const_iterator current = first; current != last; ++current)
		{
			emplace(begin() + nextInsertIndex, *current);
			++nextInsertIndex;
		}

		return begin() + insertIndex;
	}

	void pop_back()
	{
		assert(elementCount > 0 && "[vector][Assert] reason=pop_back_on_empty");
		--elementCount;
		destroyAt(storage + elementCount);
	}

	iterator erase(const_iterator position)
	{
		assert(position >= cbegin() && position < cend() && "[vector][Assert] reason=erase_position_out_of_range");
		return erase(position, position + 1);
	}

	iterator erase(const_iterator first, const_iterator last)
	{
		assert(first >= cbegin() && first <= cend() && "[vector][Assert] reason=erase_range_begin_out_of_range");
		assert(last >= first && last <= cend() && "[vector][Assert] reason=erase_range_end_out_of_range");
		const size_type eraseBeginIndex = static_cast<size_type>(first - cbegin());
		const size_type eraseEndIndex = static_cast<size_type>(last - cbegin());
		if (eraseBeginIndex >= elementCount || eraseBeginIndex >= eraseEndIndex)
		{
			return begin() + eraseBeginIndex;
		}

		const size_type eraseCount = eraseEndIndex - eraseBeginIndex;
		for (size_type moveIndex = eraseEndIndex; moveIndex < elementCount; ++moveIndex)
		{
			storage[moveIndex - eraseCount] = moveValue(storage[moveIndex]);
		}

		for (size_type destroyIndex = elementCount - eraseCount; destroyIndex < elementCount; ++destroyIndex)
		{
			destroyAt(storage + destroyIndex);
		}

		elementCount -= eraseCount;
		return begin() + eraseBeginIndex;
	}

	void clear()
	{
		if constexpr (!std::is_trivially_destructible_v<value_type>)
		{
			for (size_type elementIndex = 0; elementIndex < elementCount; ++elementIndex)
			{
				destroyAt(storage + elementIndex);
			}
		}

		elementCount = 0;
	}

	void swap(vector& other) noexcept
	{
		swapValue(storage, other.storage);
		swapValue(storageCapacity, other.storageCapacity);
		swapValue(elementCount, other.elementCount);
	}

private:
	static_assert(alignof(value_type) <= alignof(max_align_storage), "[vector][Assert] reason=overaligned_value_type_not_supported");

	using empty_storage_type = aligned_storage<sizeof(value_type), alignof(value_type)>;

	void ensureAppendCapacity()
	{
		if (elementCount < storageCapacity)
		{
			return;
		}

		size_type requestedCapacity = storageCapacity > 0 ? storageCapacity * 2 : 1;
		if (requestedCapacity <= storageCapacity)
		{
			requestedCapacity = storageCapacity + 1;
		}

		reallocate(requestedCapacity);
	}

	void reallocate(const size_type requestedCapacity)
	{
		value_type* newStorage = static_cast<value_type*>(malloc(sizeof(value_type) * requestedCapacity));
		assert(newStorage != nullptr && "[vector][Assert] reason=allocation_failed");
		for (size_type elementIndex = 0; elementIndex < elementCount; ++elementIndex)
		{
			constructAt(newStorage + elementIndex, moveValue(storage[elementIndex]));
			destroyAt(storage + elementIndex);
		}

		releaseStorage();
		storage = newStorage;
		storageCapacity = requestedCapacity;
	}

	void releaseStorage()
	{
		if (storage == nullptr)
		{
			return;
		}

		free(storage);
		storage = nullptr;
		storageCapacity = 0;
	}

	void copyFrom(const vector& other)
	{
		reserve(other.size());
		for (size_type elementIndex = 0; elementIndex < other.size(); ++elementIndex)
		{
			emplace_back(other[elementIndex]);
		}
	}

	void moveFrom(vector&& other)
	{
		storage = other.storage;
		storageCapacity = other.storageCapacity;
		elementCount = other.elementCount;
		other.storage = nullptr;
		other.storageCapacity = 0;
		other.elementCount = 0;
	}

	static value_type* getEmptyStorage()
	{
		static empty_storage_type emptyStorage = {};
		return reinterpret_cast<value_type*>(&emptyStorage);
	}

	value_type* storage = nullptr;
	size_type storageCapacity = 0;
	size_type elementCount = 0;
};

template <typename value_type>
inline bool operator==(const vector<value_type>& left, const vector<value_type>& right)
{
	if (left.size() != right.size())
	{
		return false;
	}

	for (decltype(sizeof(0)) valueIndex = 0; valueIndex < left.size(); ++valueIndex)
	{
		if (!(left[valueIndex] == right[valueIndex]))
		{
			return false;
		}
	}

	return true;
}

template <typename value_type>
inline bool operator!=(const vector<value_type>& left, const vector<value_type>& right)
{
	return !(left == right);
}

template <typename value_type>
inline enable_if<is_trivially_copyable<value_type>, OutputFileStream&>
operator<<(OutputFileStream& fileStream, const vector<value_type>& values)
{
	const uint32 valueCount = static_cast<uint32>(values.size());
	fileStream << valueCount;
	if (valueCount == 0)
	{
		return fileStream;
	}

	fileStream.write(reinterpret_cast<const char*>(values.data()), static_cast<stream_size>(sizeof(value_type) * values.size()));
	return fileStream;
}

template <typename value_type>
inline enable_if<is_trivially_copyable<value_type>, InputFileStream&>
operator>>(InputFileStream& fileStream, vector<value_type>& values)
{
	values.clear();

	uint32 valueCount = 0;
	fileStream >> valueCount;
	if (valueCount == 0)
	{
		return fileStream;
	}

	values.resizeUninitialized(valueCount);
	fileStream.read(reinterpret_cast<char*>(values.data()), static_cast<stream_size>(sizeof(value_type) * values.size()));
	return fileStream;
}
