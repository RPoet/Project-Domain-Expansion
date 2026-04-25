#pragma once

#include "Render/ResourceState.h"
#include "Render/ResourceTypes.h"


struct MapRange
{
	uint32 start;
	uint32 end;
};

template<typename PlatformResourceType>
struct PlatformResourceTraits;

template<typename BaseType, typename PlatformResourceType>
class UnderlyingResource : public BaseType
{
public:
	com_pointer<PlatformResourceType>& getUnderlyingResource() { return resource; }
	const com_pointer<PlatformResourceType>& getUnderlyingResource() const { return resource; }

	const void* getNativeResource() const override final { return resource.Get(); }
	void* getNativeResource() override final { return resource.Get(); }
	void* map(uint32 subResource, const MapRange& range) override final { return PlatformResourceTraits<PlatformResourceType>::map(resource, subResource, range); }
	void unmap(uint32 subResource, const MapRange& range) override final { return PlatformResourceTraits<PlatformResourceType>::unmap(resource, subResource, range); }
	void unmap(uint32 subResource) override final { return PlatformResourceTraits<PlatformResourceType>::unmap(resource, subResource); }
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
	virtual void* map(uint32 subResource, const MapRange& range) { return nullptr; }
	virtual void unmap(uint32 subResource, const MapRange& range) { unused(subResource); unused(range); }
	virtual void unmap(uint32 subResource) { unused(subResource); }
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

struct BufferObjectCreateOptions
{
	uint64 sizeInBytes = 0;
	BufferObjectMemoryType memoryType = BufferObjectMemoryType::defaultHeap;
};

struct TextureObjectCreateOptions
{
	TextureDimension dimension = TextureDimension::texture2D;
	uint64 alignment = 0;
	uint64 width = 0;
	uint32 height = 1;
	uint32 depthOrArraySize = 1;
	uint32 mipLevels = 1;
	TextureFormat format = TextureFormat::unknown;
	uint32 sampleCount = 1;
	uint32 sampleQuality = 0;
	TextureLayout layout = TextureLayout::unknown;
	uint32 flags = getTextureObjectFlag(TextureObjectFlag::none);
	ResourceState initialState = ResourceState::unknown;
	float clearColors[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float clearDepth = 1.0f;
	uint32 clearStencil = 0;
};
