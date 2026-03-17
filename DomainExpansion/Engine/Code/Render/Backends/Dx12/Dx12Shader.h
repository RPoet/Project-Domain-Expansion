#pragma once

#include "Render/Shader.h"

class Dx12ShaderObject final : public ShaderObject
{
public:
	void clear()
	{
		clearAsset();
		loadRequest = {};
		byteCode.clear();
		shaderDataHash = 0;
	}

	bool initialize(
		const shared_pointer<ShaderAsset>& shaderAsset,
		const ShaderBinaryLoadRequest& dx12ShaderLoadRequest,
		vector<char>&& shaderByteCode)
	{
		clear();
		if (!initializeAsset(shaderAsset)
			|| dx12ShaderLoadRequest.targetPlatform != ShaderTargetPlatform::dx12
			|| dx12ShaderLoadRequest.binaryRelativePath.empty())
		{
			return false;
		}

		loadRequest = dx12ShaderLoadRequest;
		byteCode = moveValue(shaderByteCode);
		if (byteCode.empty())
		{
			clear();
			return false;
		}

		shaderDataHash = computeShaderByteCodeHash(byteCode);
		return true;
	}

	uint64 getShaderDataHash() const override
	{
		return shaderDataHash;
	}

	const ShaderBinaryLoadRequest& getDx12LoadRequest() const
	{
		return loadRequest;
	}

	const vector<char>& getByteCode() const
	{
		return !byteCode.empty() ? byteCode : getEmptyShaderByteCode();
	}

private:
	ShaderBinaryLoadRequest loadRequest = {};
	vector<char> byteCode = {};
	uint64 shaderDataHash = 0;
};
