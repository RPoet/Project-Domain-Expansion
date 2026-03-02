#pragma once

#include "Engine/Platform/PlatformDefine.h"

enum class TextureFormat : uint32
{
	unknown = 0,
	rgba8Unorm = 1,
	d24UnormS8Uint = 2,
	d32Float = 3,
};

enum class PipelineStateType : uint32
{
	graphics = 0,
	compute = 1,
};

enum class PipelineBlendFactor : uint32
{
	zero = 0,
	one = 1,
	sourceColor = 2,
	inverseSourceColor = 3,
	sourceAlpha = 4,
	inverseSourceAlpha = 5,
	destinationColor = 6,
	inverseDestinationColor = 7,
	destinationAlpha = 8,
	inverseDestinationAlpha = 9,
};

enum class PipelineBlendOperation : uint32
{
	add = 0,
	subtract = 1,
	reverseSubtract = 2,
	minimum = 3,
	maximum = 4,
};

enum class PipelineCompareOperation : uint32
{
	never = 0,
	less = 1,
	equal = 2,
	lessEqual = 3,
	greater = 4,
	notEqual = 5,
	greaterEqual = 6,
	always = 7,
};

enum class PipelineStencilOperation : uint32
{
	keep = 0,
	zero = 1,
	replace = 2,
	incrementClamp = 3,
	decrementClamp = 4,
	invert = 5,
	incrementWrap = 6,
	decrementWrap = 7,
};

enum class PipelineCullMode : uint32
{
	none = 0,
	front = 1,
	back = 2,
};

enum class VertexInputSemantic : uint32
{
	position = 0,
	normal = 1,
	texcoord = 2,
};

enum class VertexInputFormat : uint32
{
	float2 = 0,
	float3 = 1,
	float4 = 2,
};

enum class VertexInputClassification : uint32
{
	perVertex = 0,
	perInstance = 1,
};
