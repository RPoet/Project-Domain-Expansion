#pragma once

#include <d3d12.h>

#include "Render/Backends/Dx12/Dx12PipelineStateDesc.h"
#include "Render/Backends/PipelineStateObject.h"

class Dx12PipelineStateObject final : public PipelineStateObject
{
private:
	Dx12PipelineStateDesc platformPipelineStateDesc = {};
	com_pointer<ID3D12PipelineState> pipelineState = {};

public:
	Dx12PipelineStateObject() = default;
	Dx12PipelineStateObject(
		const Dx12PipelineStateDesc& dx12PipelineStateDesc,
		const com_pointer<ID3D12PipelineState>& nativePipelineState)
		: platformPipelineStateDesc(dx12PipelineStateDesc)
		, pipelineState(nativePipelineState)
	{
	}

	const Dx12PipelineStateDesc& getPlatformPipelineStateDesc() const
	{
		return platformPipelineStateDesc;
	}

	com_pointer<ID3D12PipelineState>& getPipelineState()
	{
		return pipelineState;
	}

	const com_pointer<ID3D12PipelineState>& getPipelineState() const
	{
		return pipelineState;
	}
};
