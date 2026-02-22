#pragma once

#include <d3d12.h>
#include "Render/ResourceObject.h"

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
