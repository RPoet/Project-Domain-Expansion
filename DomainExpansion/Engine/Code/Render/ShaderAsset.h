#pragma once

#include "Engine/Platform/PlatformDefine.h"

enum class ShaderStage : uint32
{
	unknown = 0,
	vertex = 1,
	pixel = 2,
	compute = 3,
	count = 4,
};

inline constexpr uint32 shaderStageCount = static_cast<uint32>(ShaderStage::count);

inline constexpr uint32 getShaderStageIndex(const ShaderStage shaderStage)
{
	const uint32 stageIndex = static_cast<uint32>(shaderStage);
	if (stageIndex == 0 || stageIndex >= shaderStageCount)
	{
		return uint32MaxValue;
	}

	return stageIndex;
}

inline const char* getShaderStageText(const ShaderStage shaderStage)
{
	switch (shaderStage)
	{
	case ShaderStage::vertex:
		return "vertex";
	case ShaderStage::pixel:
		return "pixel";
	case ShaderStage::compute:
		return "compute";
	default:
		return "unknown";
	}
}

struct ShaderLoadRequest
{
	ShaderStage stage = ShaderStage::unknown;
	string shaderRelativePath = {};
	string entryPoint = {};
	string profile = {};
	uint64 definesHash = 0;
};

enum class ShaderAssetState : uint32
{
	pending = 0,
	ready = 1,
	failed = 2,
};

struct ShaderAsset
{
	vector<char> byteCode = {};
};

struct ShaderAssetHandle
{
	ShaderLoadRequest loadRequest = {};
	ShaderAssetState state = ShaderAssetState::pending;
	string cacheKey = {};
	shared_pointer<ShaderAsset> shaderAsset = nullptr;
};

struct ShaderPackageVariant
{
	string name = {};
	shared_pointer<ShaderAsset> shaders[shaderStageCount] = {};

	shared_pointer<ShaderAsset> getShader(const ShaderStage shaderStage) const
	{
		const uint32 shaderStageIndex = getShaderStageIndex(shaderStage);
		if (shaderStageIndex == uint32MaxValue)
		{
			return nullptr;
		}

		return shaders[shaderStageIndex];
	}
};

enum class ShaderPackageState : uint32
{
	pending = 0,
	ready = 1,
	failed = 2,
};

struct ShaderPackageAsset
{
	string packageRelativePath = {};
	ShaderPackageState state = ShaderPackageState::pending;
	vector<ShaderPackageVariant> variants = {};
};
