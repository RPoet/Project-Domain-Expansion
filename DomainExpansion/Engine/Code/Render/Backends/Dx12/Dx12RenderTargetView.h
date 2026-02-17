#pragma once

#include <d3d12.h>
#include "Render/RenderTargetView.h"

class Dx12RenderTargetView final : public RenderTargetView
{
public:
	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = {};
};

