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

enum class ShaderTargetPlatform : uint32
{
	unknown = 0,
	dx12 = 1,
	vulkan = 2,
	metal = 3,
	count = 4,
};

inline constexpr uint32 shaderTargetPlatformCount = static_cast<uint32>(ShaderTargetPlatform::count);

inline constexpr uint32 getShaderTargetPlatformIndex(const ShaderTargetPlatform shaderTargetPlatform)
{
	const uint32 platformIndex = static_cast<uint32>(shaderTargetPlatform);
	if (platformIndex == 0 || platformIndex >= shaderTargetPlatformCount)
	{
		return uint32MaxValue;
	}

	return platformIndex;
}

inline const char* getShaderTargetPlatformText(const ShaderTargetPlatform shaderTargetPlatform)
{
	switch (shaderTargetPlatform)
	{
	case ShaderTargetPlatform::dx12:
		return "dx12";
	case ShaderTargetPlatform::vulkan:
		return "vulkan";
	case ShaderTargetPlatform::metal:
		return "metal";
	default:
		return "unknown";
	}
}

struct ShaderLoadRequest
{
	ShaderStage stage = ShaderStage::unknown;
	string sourceRelativePath = {};
	string entryPoint = {};
	uint64 definesHash = 0;
};

struct ShaderBinaryLoadRequest
{
	ShaderTargetPlatform targetPlatform = ShaderTargetPlatform::unknown;
	string binaryRelativePath = {};
	string profile = {};
};

inline const ShaderLoadRequest& getEmptyShaderLoadRequest()
{
	static const ShaderLoadRequest emptyLoadRequest = {};
	return emptyLoadRequest;
}

inline const ShaderBinaryLoadRequest& getEmptyShaderBinaryLoadRequest()
{
	static const ShaderBinaryLoadRequest emptyBinaryLoadRequest = {};
	return emptyBinaryLoadRequest;
}

inline uint64 computeShaderByteCodeHash(const vector<char>& byteCode)
{
	uint64 hashValue = platformHashOffsetBasis;
	hashValue = platformHashCombine(hashValue, static_cast<uint64>(byteCode.size()));
	for (size_t byteCodeIndex = 0; byteCodeIndex < byteCode.size(); ++byteCodeIndex)
	{
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(static_cast<unsigned char>(byteCode[byteCodeIndex])));
	}

	return hashValue;
}

inline const vector<char>& getEmptyShaderByteCode()
{
	static const vector<char> emptyByteCode = {};
	return emptyByteCode;
}

struct ShaderAsset
{
	ShaderLoadRequest loadRequest = {};

	void clear()
	{
		loadRequest = {};
	}

	bool isValid() const
	{
		return getShaderStageIndex(loadRequest.stage) != uint32MaxValue
			&& !loadRequest.sourceRelativePath.empty()
			&& !loadRequest.entryPoint.empty();
	}

	const ShaderLoadRequest& getLoadRequest() const
	{
		return loadRequest;
	}

	bool initialize(const ShaderLoadRequest& shaderLoadRequest)
	{
		loadRequest = shaderLoadRequest;
		return isValid();
	}
};

class ShaderObject
{
public:
	virtual ~ShaderObject() = default;
	virtual uint64 getShaderDataHash() const = 0;

	shared_pointer<ShaderAsset> getAsset() const
	{
		return asset;
	}

	const ShaderLoadRequest& getLoadRequest() const
	{
		return asset != nullptr ? asset->getLoadRequest() : getEmptyShaderLoadRequest();
	}

	ShaderStage getStage() const
	{
		return getLoadRequest().stage;
	}

protected:
	void clearAsset()
	{
		asset.reset();
	}

	bool initializeAsset(const shared_pointer<ShaderAsset>& shaderAsset)
	{
		clearAsset();
		if (shaderAsset == nullptr || !shaderAsset->isValid())
		{
			return false;
		}

		asset = shaderAsset;
		return true;
	}

private:
	shared_pointer<ShaderAsset> asset = nullptr;
};

struct ShaderPackageVariant
{
	string name = {};
	shared_pointer<ShaderObject> shaders[shaderStageCount] = {};
	ShaderLoadRequest shaderLoadRequests[shaderStageCount] = {};
	ShaderBinaryLoadRequest shaderBinaryLoadRequests[shaderStageCount] = {};

	shared_pointer<ShaderObject> getShader(const ShaderStage shaderStage) const
	{
		const uint32 shaderStageIndex = getShaderStageIndex(shaderStage);
		if (shaderStageIndex == uint32MaxValue)
		{
			return nullptr;
		}

		return shaders[shaderStageIndex];
	}

	const ShaderLoadRequest& getLoadRequest(const ShaderStage shaderStage) const
	{
		const uint32 shaderStageIndex = getShaderStageIndex(shaderStage);
		if (shaderStageIndex == uint32MaxValue)
		{
			return getEmptyShaderLoadRequest();
		}

		return shaderLoadRequests[shaderStageIndex];
	}

	const ShaderBinaryLoadRequest& getBinaryLoadRequest(const ShaderStage shaderStage) const
	{
		const uint32 shaderStageIndex = getShaderStageIndex(shaderStage);
		if (shaderStageIndex == uint32MaxValue)
		{
			return getEmptyShaderBinaryLoadRequest();
		}

		return shaderBinaryLoadRequests[shaderStageIndex];
	}
};

enum class ShaderHandleState : uint32
{
	pending = 0,
	ready = 1,
	failed = 2,
};

struct ShaderHandle
{
	ShaderLoadRequest loadRequest = {};
	ShaderBinaryLoadRequest binaryLoadRequest = {};
	ShaderHandleState state = ShaderHandleState::pending;
	string cacheKey = {};
	shared_pointer<ShaderObject> shader = nullptr;
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
