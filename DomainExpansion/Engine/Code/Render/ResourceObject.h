#pragma once

#include "Render/ResourceTypes.h"

template<typename BaseType, typename PlatformResourceType>
class UnderlyingResource : public BaseType
{
public:
	com_pointer<PlatformResourceType>& getUnderlyingResource() { return resource; }
	const com_pointer<PlatformResourceType>& getUnderlyingResource() const { return resource; }

	const void* getNativeResource() const override final { return resource.Get(); }
	void* getNativeResource() override final { return resource.Get(); }

protected:
	com_pointer<PlatformResourceType> resource;
};

class ResourceObject
{
public:
	virtual ~ResourceObject() = default;
	virtual ResourceObjectType getResourceObjectType() const
	{
		return ResourceObjectType::unknown;
	}

	virtual const void* getNativeResource() const { return nullptr; }
	virtual void* getNativeResource() { return nullptr; }
};

struct BufferObjectCreateOptions
{
	uint64 sizeInBytes = 0;
	BufferObjectMemoryType memoryType = BufferObjectMemoryType::defaultHeap;
};

class TextureResourceObject : public ResourceObject
{
public:
	virtual ~TextureResourceObject() = default;
	ResourceObjectType getResourceObjectType() const override
	{
		return ResourceObjectType::texture;
	}
};

class BufferResourceObject : public ResourceObject
{
public:
	virtual ~BufferResourceObject() = default;
	ResourceObjectType getResourceObjectType() const override
	{
		return ResourceObjectType::buffer;
	}
};
