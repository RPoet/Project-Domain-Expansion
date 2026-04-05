#pragma once

#include "Engine/Module/Module.h"

class DiskLoaderModule final : public StaticModule<DiskLoaderModule>
{
public:
	enum class AssetFileType : uint32
	{
		document = 0,
		binary = 1,
	};

	DiskLoaderModule()
		: StaticModule("DiskLoaderModule")
	{
	}

	bool init(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	bool openInputFileStream(const string& filePath, InputFileStream& outFileStream, const bool binary) const;
	InputFileStream openInputFileStream(const string& filePath, const bool binary) const;
	bool openOutputFileStream(
		const string& filePath,
		OutputFileStream& outFileStream,
		const bool binary,
		const bool truncate) const;
	OutputFileStream openOutputFileStream(
		const string& filePath,
		const bool binary,
		const bool truncate) const;
	bool openBinaryAssetInputFileStream(const string& assetPath, InputFileStream& outFileStream) const;
	InputFileStream openBinaryAssetInputFileStream(const string& assetPath) const;
	bool openBinaryAssetOutputFileStream(
		const string& assetPath,
		OutputFileStream& outFileStream,
		const bool truncate) const;
	OutputFileStream openBinaryAssetOutputFileStream(
		const string& assetPath,
		const bool truncate) const;
	bool ensureParentDirectory(const string& filePath) const;
	bool resolveResourcesRootPath(string& outResourcesRootPath) const;
	string sanitizeFileName(const string& fileNameText, const string& fallbackName = "NewWorld") const;
	bool resolveUniqueFilePath(
		const string& directoryPath,
		const string& fileStem,
		const string& extensionWithDot,
		string& outFilePath) const;
	bool TEMP_resolveSolutionRootPath(string& outSolutionRootPath) const;
	bool TEMP_resolveImGuiIniFilePath(string& outIniFilePath) const;
	string resolveAssetPath(const string& path, AssetFileType assetFileType) const;
	bool resolveAbsolutePathFromResources(const string& pathText, string& outAbsolutePath) const;
	string resolveAbsolutePathFromResources(const string& pathText) const;
	bool resolvePathFromResources(const string& pathText, string& outAbsolutePath) const;
	string resolvePathFromResources(const string& pathText) const;
	bool TEMP_loadRuntimeWindowResolution(uint32& outClientWidth, uint32& outClientHeight) const;
	bool TEMP_saveRuntimeWindowResolution(uint32 clientWidth, uint32 clientHeight) const;
	bool loadBinaryFile(const string& absolutePath, vector<char>& outBinaryData) const;
	bool saveBinaryFile(const string& absolutePath, const vector<char>& binaryData) const;

private:
	mutable string cachedResourcesRootPath = {};
};
