#pragma once

#include "Engine/Module/Module.h"
#include "Render/Shader.h"

class ShaderModule final : public StaticModule<ShaderModule>
{
public:
	ShaderModule()
		: StaticModule("ShaderModule")
	{
	}

	bool init(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	shared_pointer<ShaderHandle> getOrLoadShader(
		const ShaderLoadRequest& loadRequest,
		const ShaderBinaryLoadRequest& binaryLoadRequest);
	void clear();
	uint32 getCachedShaderCount() const;

private:
	ShaderBinaryLoadRequest normalizeBinaryLoadRequest(const ShaderBinaryLoadRequest& binaryLoadRequest) const;
	string buildShaderCacheKey(
		const ShaderLoadRequest& loadRequest,
		const ShaderBinaryLoadRequest& binaryLoadRequest) const;
	bool validateLoadRequest(const ShaderLoadRequest& loadRequest) const;
	bool validateBinaryLoadRequest(const ShaderBinaryLoadRequest& binaryLoadRequest) const;
	bool resolveShaderBinaryAbsolutePath(const string& binaryRelativePath, string& outAbsolutePath) const;

	unordered_map<string, shared_pointer<ShaderHandle>> shaderCache;
	ShaderTargetPlatform activeTargetPlatform = ShaderTargetPlatform::dx12;
};
