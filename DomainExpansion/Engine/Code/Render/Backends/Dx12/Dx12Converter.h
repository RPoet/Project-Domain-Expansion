#pragma once

#include <d3d12.h>
#include <d3d12sdklayers.h>

#include "Render/PipelineStateObject.h"
#include "Render/ResourceTypes.h"

inline const char* getDx12MessageSeverityText(const D3D12_MESSAGE_SEVERITY severity)
{
	switch (severity)
	{
	case D3D12_MESSAGE_SEVERITY_CORRUPTION:
		return "corruption";
	case D3D12_MESSAGE_SEVERITY_ERROR:
		return "error";
	case D3D12_MESSAGE_SEVERITY_WARNING:
		return "warning";
	case D3D12_MESSAGE_SEVERITY_INFO:
		return "info";
	case D3D12_MESSAGE_SEVERITY_MESSAGE:
		return "message";
	default:
		return "unknown";
	}
}

inline bool isDx12FailureSeverity(const D3D12_MESSAGE_SEVERITY severity)
{
	return severity == D3D12_MESSAGE_SEVERITY_CORRUPTION
		|| severity == D3D12_MESSAGE_SEVERITY_ERROR;
}

inline D3D12_HEAP_TYPE getDx12BufferHeapType(const BufferObjectMemoryType memoryType)
{
	switch (memoryType)
	{
	case BufferObjectMemoryType::uploadHeap:
		return D3D12_HEAP_TYPE_UPLOAD;
	case BufferObjectMemoryType::readbackHeap:
		return D3D12_HEAP_TYPE_READBACK;
	case BufferObjectMemoryType::defaultHeap:
	default:
		return D3D12_HEAP_TYPE_DEFAULT;
	}
}

inline D3D12_RESOURCE_STATES getDx12BufferInitialState(const BufferObjectMemoryType memoryType)
{
	switch (memoryType)
	{
	case BufferObjectMemoryType::uploadHeap:
		return D3D12_RESOURCE_STATE_GENERIC_READ;
	case BufferObjectMemoryType::readbackHeap:
		return D3D12_RESOURCE_STATE_COPY_DEST;
	case BufferObjectMemoryType::defaultHeap:
	default:
		return D3D12_RESOURCE_STATE_COMMON;
	}
}

inline D3D12_RESOURCE_STATES getDx12ResourceState(const ResourceState resourceState)
{
	if (resourceState == ResourceState::unknown)
	{
		return D3D12_RESOURCE_STATE_COMMON;
	}

	static constexpr D3D12_RESOURCE_STATES dx12ResourceStates[] = {
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_INDEX_BUFFER,
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		D3D12_RESOURCE_STATE_DEPTH_READ,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_STREAM_OUT,
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_RESOLVE_DEST,
		D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
		D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_PREDICATION,
	};
	static_assert(
		static_cast<uint32>(ResourceState::count) - 1 == static_cast<uint32>(sizeof(dx12ResourceStates) / sizeof(dx12ResourceStates[0])),
		"[Dx12Converter][Assert] reason=resource_state_table_size_mismatch");

	const uint32 resourceStateIndex = static_cast<uint32>(resourceState);
	assert(resourceStateIndex > static_cast<uint32>(ResourceState::unknown) && resourceStateIndex < static_cast<uint32>(ResourceState::count));
	if (resourceStateIndex <= static_cast<uint32>(ResourceState::unknown)
		|| resourceStateIndex >= static_cast<uint32>(ResourceState::count))
	{
		return D3D12_RESOURCE_STATE_COMMON;
	}

	return dx12ResourceStates[resourceStateIndex - 1];
}

inline D3D12_SHADER_VISIBILITY getDx12ShaderVisibility(const ShaderVisibility shaderVisibility)
{
	const uint32 shaderVisibilityFlags = getShaderVisibilityFlags(shaderVisibility);
	if (shaderVisibilityFlags == getShaderVisibilityFlags(ShaderVisibility::vertex))
	{
		return D3D12_SHADER_VISIBILITY_VERTEX;
	}

	if (shaderVisibilityFlags == getShaderVisibilityFlags(ShaderVisibility::pixel))
	{
		return D3D12_SHADER_VISIBILITY_PIXEL;
	}

	return D3D12_SHADER_VISIBILITY_ALL;
}

inline uint64 hashShaderByteCode(const ShaderAsset& shaderAsset)
{
	uint64 hashValue = platformHashOffsetBasis;
	hashValue = platformHashCombine(hashValue, static_cast<uint64>(shaderAsset.byteCode.size()));
	for (size_t byteCodeIndex = 0; byteCodeIndex < shaderAsset.byteCode.size(); ++byteCodeIndex)
	{
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(static_cast<unsigned char>(shaderAsset.byteCode[byteCodeIndex])));
	}
	return hashValue;
}

inline bool isShaderAssetReady(const shared_pointer<ShaderAsset>& shaderAsset)
{
	return shaderAsset != nullptr && !shaderAsset->byteCode.empty();
}

inline DXGI_FORMAT getDx12TextureFormat(const TextureFormat textureFormat)
{
	switch (textureFormat)
	{
	case TextureFormat::rgba8Unorm:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case TextureFormat::d24UnormS8Uint:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case TextureFormat::d32Float:
		return DXGI_FORMAT_D32_FLOAT;
	case TextureFormat::unknown:
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

inline D3D12_RESOURCE_DIMENSION getDx12TextureDimension(const TextureDimension textureDimension)
{
	switch (textureDimension)
	{
	case TextureDimension::texture1D:
		return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
	case TextureDimension::texture2D:
		return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	case TextureDimension::texture3D:
		return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	default:
		return D3D12_RESOURCE_DIMENSION_UNKNOWN;
	}
}

inline D3D12_TEXTURE_LAYOUT getDx12TextureLayout(const TextureLayout textureLayout)
{
	switch (textureLayout)
	{
	case TextureLayout::rowMajor:
		return D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	case TextureLayout::standardSwizzle64KB:
		return D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE;
	case TextureLayout::undefinedSwizzle64KB:
		return D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
	case TextureLayout::unknown:
	default:
		return D3D12_TEXTURE_LAYOUT_UNKNOWN;
	}
}

inline D3D12_RESOURCE_FLAGS getDx12TextureResourceFlags(const uint32 textureFlags)
{
	D3D12_RESOURCE_FLAGS dx12Flags = D3D12_RESOURCE_FLAG_NONE;
	if ((textureFlags & getTextureObjectFlag(TextureObjectFlag::allowRenderTarget)) != 0)
	{
		dx12Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}

	if ((textureFlags & getTextureObjectFlag(TextureObjectFlag::allowDepthStencil)) != 0)
	{
		dx12Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}

	if ((textureFlags & getTextureObjectFlag(TextureObjectFlag::allowUnorderedAccess)) != 0)
	{
		dx12Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	if ((textureFlags & getTextureObjectFlag(TextureObjectFlag::denyShaderResource)) != 0)
	{
		dx12Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	if ((textureFlags & getTextureObjectFlag(TextureObjectFlag::allowCrossAdapter)) != 0)
	{
		dx12Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
	}

	if ((textureFlags & getTextureObjectFlag(TextureObjectFlag::allowSimultaneousAccess)) != 0)
	{
		dx12Flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
	}

	return dx12Flags;
}

inline D3D12_BLEND getDx12BlendFactor(const PipelineBlendFactor blendFactor)
{
	switch (blendFactor)
	{
	case PipelineBlendFactor::zero:
		return D3D12_BLEND_ZERO;
	case PipelineBlendFactor::one:
		return D3D12_BLEND_ONE;
	case PipelineBlendFactor::sourceColor:
		return D3D12_BLEND_SRC_COLOR;
	case PipelineBlendFactor::inverseSourceColor:
		return D3D12_BLEND_INV_SRC_COLOR;
	case PipelineBlendFactor::sourceAlpha:
		return D3D12_BLEND_SRC_ALPHA;
	case PipelineBlendFactor::inverseSourceAlpha:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case PipelineBlendFactor::destinationColor:
		return D3D12_BLEND_DEST_COLOR;
	case PipelineBlendFactor::inverseDestinationColor:
		return D3D12_BLEND_INV_DEST_COLOR;
	case PipelineBlendFactor::destinationAlpha:
		return D3D12_BLEND_DEST_ALPHA;
	case PipelineBlendFactor::inverseDestinationAlpha:
		return D3D12_BLEND_INV_DEST_ALPHA;
	default:
		return D3D12_BLEND_ONE;
	}
}

inline D3D12_BLEND_OP getDx12BlendOperation(const PipelineBlendOperation blendOperation)
{
	switch (blendOperation)
	{
	case PipelineBlendOperation::add:
		return D3D12_BLEND_OP_ADD;
	case PipelineBlendOperation::subtract:
		return D3D12_BLEND_OP_SUBTRACT;
	case PipelineBlendOperation::reverseSubtract:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case PipelineBlendOperation::minimum:
		return D3D12_BLEND_OP_MIN;
	case PipelineBlendOperation::maximum:
		return D3D12_BLEND_OP_MAX;
	default:
		return D3D12_BLEND_OP_ADD;
	}
}

inline D3D12_COMPARISON_FUNC getDx12CompareOperation(const PipelineCompareOperation compareOperation)
{
	switch (compareOperation)
	{
	case PipelineCompareOperation::never:
		return D3D12_COMPARISON_FUNC_NEVER;
	case PipelineCompareOperation::less:
		return D3D12_COMPARISON_FUNC_LESS;
	case PipelineCompareOperation::equal:
		return D3D12_COMPARISON_FUNC_EQUAL;
	case PipelineCompareOperation::lessEqual:
		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case PipelineCompareOperation::greater:
		return D3D12_COMPARISON_FUNC_GREATER;
	case PipelineCompareOperation::notEqual:
		return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case PipelineCompareOperation::greaterEqual:
		return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case PipelineCompareOperation::always:
	default:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	}
}

inline D3D12_STENCIL_OP getDx12StencilOperation(const PipelineStencilOperation stencilOperation)
{
	switch (stencilOperation)
	{
	case PipelineStencilOperation::keep:
		return D3D12_STENCIL_OP_KEEP;
	case PipelineStencilOperation::zero:
		return D3D12_STENCIL_OP_ZERO;
	case PipelineStencilOperation::replace:
		return D3D12_STENCIL_OP_REPLACE;
	case PipelineStencilOperation::incrementClamp:
		return D3D12_STENCIL_OP_INCR_SAT;
	case PipelineStencilOperation::decrementClamp:
		return D3D12_STENCIL_OP_DECR_SAT;
	case PipelineStencilOperation::invert:
		return D3D12_STENCIL_OP_INVERT;
	case PipelineStencilOperation::incrementWrap:
		return D3D12_STENCIL_OP_INCR;
	case PipelineStencilOperation::decrementWrap:
		return D3D12_STENCIL_OP_DECR;
	default:
		return D3D12_STENCIL_OP_KEEP;
	}
}

inline D3D12_RENDER_TARGET_BLEND_DESC getDx12RenderTargetBlendDesc(const PipelineRenderTargetBlendDesc& blendDesc)
{
	D3D12_RENDER_TARGET_BLEND_DESC dx12BlendDesc = {};
	dx12BlendDesc.BlendEnable = blendDesc.blendEnabled ? boolTrue : boolFalse;
	dx12BlendDesc.LogicOpEnable = boolFalse;
	dx12BlendDesc.SrcBlend = getDx12BlendFactor(blendDesc.sourceColorBlendFactor);
	dx12BlendDesc.DestBlend = getDx12BlendFactor(blendDesc.destinationColorBlendFactor);
	dx12BlendDesc.BlendOp = getDx12BlendOperation(blendDesc.colorBlendOperation);
	dx12BlendDesc.SrcBlendAlpha = getDx12BlendFactor(blendDesc.sourceAlphaBlendFactor);
	dx12BlendDesc.DestBlendAlpha = getDx12BlendFactor(blendDesc.destinationAlphaBlendFactor);
	dx12BlendDesc.BlendOpAlpha = getDx12BlendOperation(blendDesc.alphaBlendOperation);
	dx12BlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	dx12BlendDesc.RenderTargetWriteMask = static_cast<UINT8>(blendDesc.colorWriteMask & 0x0Fu);
	return dx12BlendDesc;
}

inline D3D12_CULL_MODE getDx12CullMode(const PipelineCullMode cullMode)
{
	switch (cullMode)
	{
	case PipelineCullMode::none:
		return D3D12_CULL_MODE_NONE;
	case PipelineCullMode::front:
		return D3D12_CULL_MODE_FRONT;
	case PipelineCullMode::back:
	default:
		return D3D12_CULL_MODE_BACK;
	}
}

inline const char* getDx12VertexInputSemanticName(const VertexInputSemantic semantic)
{
	switch (semantic)
	{
	case VertexInputSemantic::position:
		return "POSITION";
	case VertexInputSemantic::normal:
		return "NORMAL";
	case VertexInputSemantic::texcoord:
		return "TEXCOORD";
	default:
		return nullptr;
	}
}

inline DXGI_FORMAT getDx12VertexInputFormat(const VertexInputFormat format)
{
	switch (format)
	{
	case VertexInputFormat::float2:
		return DXGI_FORMAT_R32G32_FLOAT;
	case VertexInputFormat::float3:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case VertexInputFormat::float4:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

inline D3D12_INPUT_CLASSIFICATION getDx12VertexInputClassification(const VertexInputClassification inputClassification)
{
	switch (inputClassification)
	{
	case VertexInputClassification::perInstance:
		return D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
	case VertexInputClassification::perVertex:
	default:
		return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	}
}
