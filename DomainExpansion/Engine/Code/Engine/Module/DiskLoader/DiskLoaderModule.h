#pragma once

#include "Engine/Module/Module.h"

class DiskLoaderModule final : public StaticModule<DiskLoaderModule>
{
public:
	DiskLoaderModule()
		: StaticModule("DiskLoaderModule")
	{
	}

	bool init(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	bool ensureParentDirectory(const string& filePath) const;
	bool TEMP_resolveSolutionRootPath(string& outSolutionRootPath) const;
	bool TEMP_resolveImGuiIniFilePath(string& outIniFilePath) const;
	bool resolvePathFromResources(const string& pathText, string& outAbsolutePath) const;
	bool TEMP_loadRuntimeWindowResolution(uint32& outClientWidth, uint32& outClientHeight) const;
	bool TEMP_saveRuntimeWindowResolution(uint32 clientWidth, uint32 clientHeight) const;
	bool loadBinaryFile(const string& absolutePath, vector<char>& outBinaryData) const;
	bool saveBinaryFile(const string& absolutePath, const vector<char>& binaryData) const;
};
