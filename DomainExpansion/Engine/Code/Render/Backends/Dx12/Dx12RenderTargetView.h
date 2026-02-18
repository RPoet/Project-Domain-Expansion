#pragma once

#include <d3d12.h>
#include "Render/RenderTargetView.h"

class Dx12RenderTargetView final : public RenderTargetView
{
public:
	// TO DO : Remove direct descriptor exposure after descriptor/view allocator refactor.
	com_pointer<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = {};
};
