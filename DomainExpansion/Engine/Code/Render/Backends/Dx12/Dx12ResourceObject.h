#pragma once

#include <d3d12.h>
#include "Render/Backends/ResourceObject.h"

template<>
struct PlatformResourceTraits<ID3D12Resource>
{
	static void* map(com_pointer<ID3D12Resource>& resource, uint32 subResource, const MapRange& range)
	{
		void* mapped = nullptr;
		D3D12_RANGE readRange
		{
			.Begin = static_cast<SIZE_T>(range.start),
			.End = static_cast<SIZE_T>(range.end)
		};

		resource->Map(subResource, &readRange, &mapped);
		assert(mapped != nullptr && "[d3d12] resource map func failed");
		return mapped;
	}

	static void unmap(com_pointer<ID3D12Resource>& resource, uint32 subResource, const MapRange& range)
	{
		D3D12_RANGE writtenRange
		{
			.Begin = static_cast<SIZE_T>(range.start),
			.End = static_cast<SIZE_T>(range.end)
		};

		resource->Unmap(subResource, &writtenRange);
	}

	static void unmap(com_pointer<ID3D12Resource>& resource, uint32 subResource)
	{
		resource->Unmap(subResource, nullptr);
	}
};

class Dx12ResourceObject final : public UnderlyingResource<ResourceObject, ID3D12Resource>
{
public:
};

class Dx12BufferObject final : public UnderlyingResource<BufferResourceObject, ID3D12Resource>
{
public:
};

class Dx12TextureResourceObject final : public UnderlyingResource<TextureResourceObject, ID3D12Resource>
{
public:
};
