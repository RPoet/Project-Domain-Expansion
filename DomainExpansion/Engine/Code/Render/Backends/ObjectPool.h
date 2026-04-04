#pragma once

#include "Engine/Platform/PlatformDefine.h"

template <typename object_type>
inline constexpr bool objectPoolUsesComPointer = requires(object_type* releaseCandidate) { releaseCandidate->Release(); };

template <typename object_type>
using object_pool_owned_pointer = std::conditional_t<objectPoolUsesComPointer<object_type>, com_pointer<object_type>, unique_pointer<object_type>>;

template <typename object_type>
class ObjectPool final
{
public:
	~ObjectPool()
	{
		clear();
	}

	void reserve(const uint32 objectCapacity)
	{
		ownedObjects.reserve(objectCapacity);
		availableObjects.reserve(objectCapacity);
	}

	template <typename create_function_type>
	object_type* acquireOrCreate(create_function_type&& createObjectFunction)
	{
		if (!availableObjects.empty())
		{
			object_type* object = availableObjects.back();
			availableObjects.pop_back();
			return object;
		}

		object_type* createdObject = createObjectFunction();
		if (createdObject == nullptr)
		{
			return nullptr;
		}

		ownedObjects.push_back(makeOwnedObject(createdObject));
		return createdObject;
	}

	void retreieve(object_type* object)
	{
		if (object == nullptr)
		{
			return;
		}

		assert(containsOwnedObject(object) && "[ObjectPool][Assert] reason=object_not_owned");
		assert(!containsAvailableObject(object) && "[ObjectPool][Assert] reason=object_already_available");
		availableObjects.push_back(object);
	}

	void clear()
	{
		if constexpr (!objectPoolUsesComPointer<object_type>)
		{
			for (uint32 objectIndex = 0; objectIndex < static_cast<uint32>(ownedObjects.size()); ++objectIndex)
			{
				shutdownOwnedObject(getOwnedObjectPointer(ownedObjects[objectIndex]));
			}
		}

		availableObjects.clear();
		ownedObjects.clear();
	}

private:
	using owned_pointer_type = object_pool_owned_pointer<object_type>;

	static owned_pointer_type makeOwnedObject(object_type* object)
	{
		if constexpr (objectPoolUsesComPointer<object_type>)
		{
			owned_pointer_type ownedObject = nullptr;
			*ownedObject.ReleaseAndGetAddressOf() = object;
			return ownedObject;
		}

		return owned_pointer_type(object);
	}

	static object_type* getOwnedObjectPointer(const owned_pointer_type& ownedObject)
	{
		if constexpr (objectPoolUsesComPointer<object_type>)
		{
			return ownedObject.Get();
		}
		else
		{
			return ownedObject.get();
		}
	}

	static void shutdownOwnedObject(object_type* object)
	{
		if (object == nullptr)
		{
			return;
		}

		if constexpr (requires(object_type* shutdownCandidate) { shutdownCandidate->shutdown(); })
		{
			object->shutdown();
		}
	}

	bool containsOwnedObject(const object_type* object) const
	{
		for (uint32 objectIndex = 0; objectIndex < static_cast<uint32>(ownedObjects.size()); ++objectIndex)
		{
			if (getOwnedObjectPointer(ownedObjects[objectIndex]) == object)
			{
				return true;
			}
		}

		return false;
	}

	bool containsAvailableObject(const object_type* object) const
	{
		for (uint32 objectIndex = 0; objectIndex < static_cast<uint32>(availableObjects.size()); ++objectIndex)
		{
			if (availableObjects[objectIndex] == object)
			{
				return true;
			}
		}

		return false;
	}

	vector<owned_pointer_type> ownedObjects = {};
	vector<object_type*> availableObjects = {};
};
