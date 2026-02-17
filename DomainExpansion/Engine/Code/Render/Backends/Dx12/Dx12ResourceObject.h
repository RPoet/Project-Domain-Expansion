#pragma once

#include <d3d12.h>
#include "Render/ResourceObject.h"

class Dx12ResourceObject final : public ResourceObject
{
public:
	com_pointer<ID3D12Resource> resource;
};
