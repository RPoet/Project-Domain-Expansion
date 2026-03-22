#pragma once

#include "Engine/Module/Shader/ShaderModule.h"
#include "Engine/Module/Module.h"
#include "Render/Shader.h"

class ShaderPackageModule final : public StaticModule<ShaderPackageModule>
{
public:
	ShaderPackageModule()
		: StaticModule("ShaderPackageModule")
	{
	}

	bool init(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	shared_pointer<ShaderPackageAsset> getOrLoadPackage(const string& packageRelativePath);
	void clear();
	uint32 getCachedPackageCount() const;

private:
	bool resolvePackageAbsolutePath(const string& packageRelativePath, string& outAbsolutePath) const;
	string buildPackageCacheKey(const string& packageRelativePath) const;

	unordered_map<string, shared_pointer<ShaderPackageAsset>> packageCache;
};
