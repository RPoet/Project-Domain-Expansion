#pragma once

#include "Render/Backends/RenderBackendDefinitions.h"
#include "Render/RenderTypes.h"
#include "Render/RootSignatureObject.h"
#include "Render/ShaderAsset.h"

inline constexpr uint32 pipelineStateDescInputElementInlineCapacity = 8;

struct PipelineRenderTargetBlendDesc
{
	bool blendEnabled = false;
	PipelineBlendFactor sourceColorBlendFactor = PipelineBlendFactor::one;
	PipelineBlendFactor destinationColorBlendFactor = PipelineBlendFactor::zero;
	PipelineBlendOperation colorBlendOperation = PipelineBlendOperation::add;
	PipelineBlendFactor sourceAlphaBlendFactor = PipelineBlendFactor::one;
	PipelineBlendFactor destinationAlphaBlendFactor = PipelineBlendFactor::zero;
	PipelineBlendOperation alphaBlendOperation = PipelineBlendOperation::add;
	uint32 colorWriteMask = 0x0Fu;

	bool operator==(const PipelineRenderTargetBlendDesc& other) const
	{
		return blendEnabled == other.blendEnabled
			&& sourceColorBlendFactor == other.sourceColorBlendFactor
			&& destinationColorBlendFactor == other.destinationColorBlendFactor
			&& colorBlendOperation == other.colorBlendOperation
			&& sourceAlphaBlendFactor == other.sourceAlphaBlendFactor
			&& destinationAlphaBlendFactor == other.destinationAlphaBlendFactor
			&& alphaBlendOperation == other.alphaBlendOperation
			&& colorWriteMask == other.colorWriteMask;
	}

	uint64 getHashValue() const
	{
		uint64 hashValue = platformHashOffsetBasis;
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(blendEnabled));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(sourceColorBlendFactor));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(destinationColorBlendFactor));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(colorBlendOperation));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(sourceAlphaBlendFactor));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(destinationAlphaBlendFactor));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(alphaBlendOperation));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(colorWriteMask));
		return hashValue;
	}
};

struct PipelineRenderTargetDesc
{
	TextureFormat colorFormat = TextureFormat::rgba8Unorm;
	PipelineRenderTargetBlendDesc blendDesc = {};

	bool operator==(const PipelineRenderTargetDesc& other) const
	{
		return colorFormat == other.colorFormat && blendDesc == other.blendDesc;
	}

	uint64 getHashValue() const
	{
		uint64 hashValue = platformHashOffsetBasis;
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(colorFormat));
		hashValue = platformHashCombine(hashValue, blendDesc.getHashValue());
		return hashValue;
	}
};

struct PipelineDepthStencilFaceDesc
{
	PipelineStencilOperation stencilFailOperation = PipelineStencilOperation::keep;
	PipelineStencilOperation stencilDepthFailOperation = PipelineStencilOperation::keep;
	PipelineStencilOperation stencilPassOperation = PipelineStencilOperation::keep;
	PipelineCompareOperation stencilCompareOperation = PipelineCompareOperation::always;

	bool operator==(const PipelineDepthStencilFaceDesc& other) const
	{
		return stencilFailOperation == other.stencilFailOperation
			&& stencilDepthFailOperation == other.stencilDepthFailOperation
			&& stencilPassOperation == other.stencilPassOperation
			&& stencilCompareOperation == other.stencilCompareOperation;
	}

	uint64 getHashValue() const
	{
		uint64 hashValue = platformHashOffsetBasis;
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(stencilFailOperation));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(stencilDepthFailOperation));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(stencilPassOperation));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(stencilCompareOperation));
		return hashValue;
	}
};

struct PipelineDepthStencilDesc
{
	TextureFormat depthStencilFormat = TextureFormat::unknown;
	bool depthTestEnabled = false;
	bool depthWriteEnabled = false;
	PipelineCompareOperation depthCompareOperation = PipelineCompareOperation::lessEqual;
	bool stencilEnabled = false;
	uint32 stencilReadMask = 0xFFu;
	uint32 stencilWriteMask = 0xFFu;
	PipelineDepthStencilFaceDesc frontFace = {};
	PipelineDepthStencilFaceDesc backFace = {};

	bool operator==(const PipelineDepthStencilDesc& other) const
	{
		return depthStencilFormat == other.depthStencilFormat
			&& depthTestEnabled == other.depthTestEnabled
			&& depthWriteEnabled == other.depthWriteEnabled
			&& depthCompareOperation == other.depthCompareOperation
			&& stencilEnabled == other.stencilEnabled
			&& stencilReadMask == other.stencilReadMask
			&& stencilWriteMask == other.stencilWriteMask
			&& frontFace == other.frontFace
			&& backFace == other.backFace;
	}

	uint64 getHashValue() const
	{
		uint64 hashValue = platformHashOffsetBasis;
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(depthStencilFormat));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(depthTestEnabled));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(depthWriteEnabled));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(depthCompareOperation));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(stencilEnabled));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(stencilReadMask));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(stencilWriteMask));
		hashValue = platformHashCombine(hashValue, frontFace.getHashValue());
		hashValue = platformHashCombine(hashValue, backFace.getHashValue());
		return hashValue;
	}
};

struct PipelineInputElementDesc
{
	VertexInputSemantic semantic = VertexInputSemantic::position;
	uint32 semanticIndex = 0;
	VertexInputFormat format = VertexInputFormat::float3;
	uint32 inputSlot = 0;
	uint32 alignedByteOffset = 0;
	VertexInputClassification inputClassification = VertexInputClassification::perVertex;
	uint32 instanceDataStepRate = 0;

	bool operator==(const PipelineInputElementDesc& other) const
	{
		return semantic == other.semantic
			&& semanticIndex == other.semanticIndex
			&& format == other.format
			&& inputSlot == other.inputSlot
			&& alignedByteOffset == other.alignedByteOffset
			&& inputClassification == other.inputClassification
			&& instanceDataStepRate == other.instanceDataStepRate;
	}

	uint64 getHashValue() const
	{
		uint64 hashValue = platformHashOffsetBasis;
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(semantic));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(semanticIndex));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(format));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(inputSlot));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(alignedByteOffset));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(inputClassification));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(instanceDataStepRate));
		return hashValue;
	}
};

struct PipelineStateDesc
{
	PipelineStateType pipelineStateType = PipelineStateType::graphics;
	RootSignatureDesc rootSignatureDesc = {};
	shared_pointer<ShaderAsset> vertexShader = nullptr;
	shared_pointer<ShaderAsset> pixelShader = nullptr;
	shared_pointer<ShaderAsset> computeShader = nullptr;
	InplaceVector<PipelineInputElementDesc, pipelineStateDescInputElementInlineCapacity> inputElements = {};
	bool wireframe = false;
	uint32 sampleCount = 1;
	InplaceVector<PipelineRenderTargetDesc, renderBackendRenderTargetSlotCount> renderTargets = {};
	PipelineDepthStencilDesc depthStencilDesc = {};
	PipelineCullMode cullMode = PipelineCullMode::back;
};

class PipelineStateObject
{
public:
	virtual ~PipelineStateObject() = default;
};
