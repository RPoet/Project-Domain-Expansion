#pragma once

#include <d3d12.h>

#include "Render/DepthStencilView.h"

class Dx12DepthStencilView final : public DepthStencilView
{
public:
	// TO DO : Refactor descriptor heap ownership out of individual depth-stencil view objects.
	com_pointer<ID3D12DescriptorHeap> descriptorHeap = {};
	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = {};
	TextureFormat textureFormat = TextureFormat::unknown;

	TextureFormat getTextureFormat() const override
	{
		return textureFormat;
	}
};
