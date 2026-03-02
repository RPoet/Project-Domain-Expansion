#pragma once

#include "Engine/Module/Module.h"
#include "Render/ShaderAsset.h"

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

	shared_pointer<ShaderAssetHandle> getOrLoadShader(const ShaderLoadRequest& loadRequest);
	void clear();
	uint32 getCachedShaderCount() const;

private:
	string buildShaderCacheKey(const ShaderLoadRequest& loadRequest) const;
	bool validateLoadRequest(const ShaderLoadRequest& loadRequest) const;
	bool resolveShaderAbsolutePath(const string& shaderRelativePath, string& outAbsolutePath) const;

	unordered_map<string, shared_pointer<ShaderAssetHandle>> shaderCache;
};
