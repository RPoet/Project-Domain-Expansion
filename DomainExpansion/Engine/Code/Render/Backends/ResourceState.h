#pragma once

#include "Engine/Platform/PlatformDefine.h"

enum class ResourceState : uint32
{
	unknown = 0,
	common = 1,
	vertexAndConstantBuffer = 2,
	indexBuffer = 3,
	renderTarget = 4,
	unorderedAccess = 5,
	depthWrite = 6,
	depthRead = 7,
	nonPixelShaderResource = 8,
	pixelShaderResource = 9,
	streamOut = 10,
	indirectArgument = 11,
	copyDest = 12,
	copySource = 13,
	resolveDest = 14,
	resolveSource = 15,
	raytracingAccelerationStructure = 16,
	shadingRateSource = 17,
	genericRead = 18,
	allShaderResource = 19,
	present = 20,
	predication = 21,
	count = 22,
};
