#pragma once

#include "Engine/Platform/PlatformDefine.h"

enum class ShaderVisibility : uint32
{
	none = 0,
	vertex = static_cast<uint32>(1u << 0),
	pixel = static_cast<uint32>(1u << 1),
	allGraphics = static_cast<uint32>((1u << 0) | (1u << 1)),
};

inline constexpr uint32 getShaderVisibilityFlags(const ShaderVisibility shaderVisibility)
{
	return static_cast<uint32>(shaderVisibility);
}

struct PushConstantRange
{
	uint32 offsetInBytes = 0;
	uint32 sizeInBytes = 0;
	ShaderVisibility shaderVisibility = ShaderVisibility::allGraphics;
};

struct RootSignatureDesc
{
	InplaceVector<PushConstantRange, 8> pushConstantRanges = {};
};

class RootSignatureObject
{
public:
	virtual ~RootSignatureObject() = default;
};
