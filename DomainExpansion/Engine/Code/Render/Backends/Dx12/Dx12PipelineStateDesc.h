#pragma once

#include <d3d12.h>

#include "Render/Backends/PipelineStateObject.h"

struct Dx12PipelineStateDesc
{
	PipelineStateType pipelineStateType = PipelineStateType::graphics;
	uint64 rootSignatureHash = 0;
	uint64 vertexShaderHash = 0;
	uint64 pixelShaderHash = 0;
	uint64 computeShaderHash = 0;
	uint32 vertexShaderByteCodeSize = 0;
	uint32 pixelShaderByteCodeSize = 0;
	uint32 computeShaderByteCodeSize = 0;
	InplaceVector<PipelineInputElementDesc, pipelineStateDescInputElementInlineCapacity> inputElements = {};
	bool wireframe = false;
	uint32 sampleCount = 1;
	InplaceVector<PipelineRenderTargetDesc, renderBackendRenderTargetSlotCount> renderTargets = {};
	PipelineDepthStencilDesc depthStencilDesc = {};
	PipelineCullMode cullMode = PipelineCullMode::back;

	bool operator==(const Dx12PipelineStateDesc& other) const
	{
		const bool stateMatched = pipelineStateType == other.pipelineStateType
			&& rootSignatureHash == other.rootSignatureHash
			&& vertexShaderHash == other.vertexShaderHash
			&& pixelShaderHash == other.pixelShaderHash
			&& computeShaderHash == other.computeShaderHash
			&& vertexShaderByteCodeSize == other.vertexShaderByteCodeSize
			&& pixelShaderByteCodeSize == other.pixelShaderByteCodeSize
			&& computeShaderByteCodeSize == other.computeShaderByteCodeSize
			&& wireframe == other.wireframe
			&& sampleCount == other.sampleCount
			&& depthStencilDesc == other.depthStencilDesc
			&& cullMode == other.cullMode;
		if (!stateMatched
			|| inputElements.size() != other.inputElements.size()
			|| renderTargets.size() != other.renderTargets.size())
		{
			return false;
		}

		for (uint32 elementIndex = 0; elementIndex < static_cast<uint32>(inputElements.size()); ++elementIndex)
		{
			if (!(inputElements[elementIndex] == other.inputElements[elementIndex]))
			{
				return false;
			}
		}

		for (uint32 renderTargetIndex = 0; renderTargetIndex < static_cast<uint32>(renderTargets.size()); ++renderTargetIndex)
		{
			if (!(renderTargets[renderTargetIndex] == other.renderTargets[renderTargetIndex]))
			{
				return false;
			}
		}

		return true;
	}

	uint64 getHashValue() const
	{
		uint64 hashValue = platformHashOffsetBasis;
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(pipelineStateType));
		hashValue = platformHashCombine(hashValue, rootSignatureHash);
		hashValue = platformHashCombine(hashValue, vertexShaderHash);
		hashValue = platformHashCombine(hashValue, pixelShaderHash);
		hashValue = platformHashCombine(hashValue, computeShaderHash);
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(vertexShaderByteCodeSize));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(pixelShaderByteCodeSize));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(computeShaderByteCodeSize));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(inputElements.size()));
		for (uint32 elementIndex = 0; elementIndex < static_cast<uint32>(inputElements.size()); ++elementIndex)
		{
			const PipelineInputElementDesc& inputElement = inputElements[elementIndex];
			hashValue = platformHashCombine(hashValue, inputElement.getHashValue());
		}
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(wireframe));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(sampleCount));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(renderTargets.size()));
		for (uint32 renderTargetIndex = 0; renderTargetIndex < static_cast<uint32>(renderTargets.size()); ++renderTargetIndex)
		{
			hashValue = platformHashCombine(hashValue, renderTargets[renderTargetIndex].getHashValue());
		}
		hashValue = platformHashCombine(hashValue, depthStencilDesc.getHashValue());
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(cullMode));
		return hashValue;
	}
};
