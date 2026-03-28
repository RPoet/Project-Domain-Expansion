#include "Engine/Module/DiskLoader/DiskLoaderModule.h"

#include "Engine/Framework/FrameworkFileSystem.h"

#include <fstream>

static bool TEMP_parseDiskLoaderUnsignedInteger(const string& textValue, uint32& parsedValue)
{
	parsedValue = 0;
	if (textValue.empty())
	{
		return false;
	}

	uint64 value = 0;
	for (size_t characterIndex = 0; characterIndex < textValue.length(); ++characterIndex)
	{
		const char character = textValue[characterIndex];
		if (character < '0' || character > '9')
		{
			return false;
		}

		value = (value * 10ull) + static_cast<uint64>(character - '0');
		if (value > static_cast<uint64>(uint32MaxValue))
		{
			return false;
		}
	}

	parsedValue = static_cast<uint32>(value);
	return true;
}

static string TEMP_trimDiskLoaderIniToken(const string& text)
{
	size_t beginIndex = 0;
	while (beginIndex < text.length() && (text[beginIndex] == ' ' || text[beginIndex] == '\t'))
	{
		++beginIndex;
	}

	size_t endIndex = text.length();
	while (endIndex > beginIndex && (text[endIndex - 1] == ' ' || text[endIndex - 1] == '\t'))
	{
		--endIndex;
	}

	return text.substr(beginIndex, endIndex - beginIndex);
}

static bool TEMP_tryParseDiskLoaderIniUnsignedInteger(
	const string& lineText,
	const char* keyText,
	uint32& outValue)
{
	const size_t separatorIndex = lineText.find('=');
	if (separatorIndex == string::npos)
	{
		return false;
	}

	const string key = TEMP_trimDiskLoaderIniToken(lineText.substr(0, separatorIndex));
	if (key != keyText)
	{
		return false;
	}

	return TEMP_parseDiskLoaderUnsignedInteger(
		TEMP_trimDiskLoaderIniToken(lineText.substr(separatorIndex + 1)),
		outValue);
}

static bool TEMP_resolveDiskLoaderRuntimeIniFilePath(string& outIniFilePath)
{
	outIniFilePath.clear();
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	string solutionRootPath = {};
	if (!diskLoaderModule->TEMP_resolveSolutionRootPath(solutionRootPath))
	{
		return false;
	}

	outIniFilePath = (filesystem_path(solutionRootPath) / "runtime.ini")
		.lexically_normal()
		.string();
	return true;
}

bool DiskLoaderModule::init(Framework& framework)
{
	unused(framework);
	return true;
}

void DiskLoaderModule::preUpdate()
{
}

void DiskLoaderModule::postUpdate()
{
}

void DiskLoaderModule::shutdown()
{
}

bool DiskLoaderModule::openInputFileStream(
	const string& filePath,
	InputFileStream& outFileStream,
	const bool binary) const
{
	outFileStream.close();
	outFileStream.clear();
	if (filePath.empty())
	{
		return false;
	}

	input_file_stream::openmode openMode = input_file_stream::in;
	if (binary)
	{
		openMode |= input_file_stream::binary;
	}

	outFileStream.open(filePath, openMode);
	return outFileStream.is_open() && outFileStream.good();
}

InputFileStream DiskLoaderModule::openInputFileStream(const string& filePath, const bool binary) const
{
	InputFileStream fileStream = {};
	const bool openedFileStream = openInputFileStream(filePath, fileStream, binary);
	assert(openedFileStream && "[DiskLoaderModule][Assert] reason=input_file_open_failed");
	return fileStream;
}

bool DiskLoaderModule::openOutputFileStream(
	const string& filePath,
	OutputFileStream& outFileStream,
	const bool binary,
	const bool truncate) const
{
	outFileStream.close();
	outFileStream.clear();
	if (filePath.empty() || !ensureParentDirectory(filePath))
	{
		return false;
	}

	output_file_stream::openmode openMode = output_file_stream::out;
	if (binary)
	{
		openMode |= output_file_stream::binary;
	}

	if (truncate)
	{
		openMode |= output_file_stream::trunc;
	}

	outFileStream.open(filePath, openMode);
	return outFileStream.is_open() && outFileStream.good();
}

OutputFileStream DiskLoaderModule::openOutputFileStream(
	const string& filePath,
	const bool binary,
	const bool truncate) const
{
	OutputFileStream fileStream = {};
	const bool openedFileStream = openOutputFileStream(filePath, fileStream, binary, truncate);
	assert(openedFileStream && "[DiskLoaderModule][Assert] reason=output_file_open_failed");
	return fileStream;
}

bool DiskLoaderModule::openBinaryAssetInputFileStream(
	const string& assetPath,
	InputFileStream& outFileStream) const
{
	const string binaryAssetPath = resolveAssetPath(assetPath, AssetFileType::binary);
	string binaryAbsolutePath = {};
	const bool validBinaryAbsolutePath = resolveAbsolutePathFromResources(binaryAssetPath, binaryAbsolutePath);
	if (!validBinaryAbsolutePath)
	{
		return false;
	}

	return openInputFileStream(binaryAbsolutePath, outFileStream, true);
}

InputFileStream DiskLoaderModule::openBinaryAssetInputFileStream(const string& assetPath) const
{
	InputFileStream fileStream = {};
	const bool openedFileStream = openBinaryAssetInputFileStream(assetPath, fileStream);
	assert(openedFileStream && "[DiskLoaderModule][Assert] reason=binary_asset_input_file_open_failed");
	return fileStream;
}

bool DiskLoaderModule::openBinaryAssetOutputFileStream(
	const string& assetPath,
	OutputFileStream& outFileStream,
	const bool truncate) const
{
	const string binaryAssetPath = resolveAssetPath(assetPath, AssetFileType::binary);
	string binaryAbsolutePath = {};
	const bool validBinaryAbsolutePath = resolveAbsolutePathFromResources(binaryAssetPath, binaryAbsolutePath);
	if (!validBinaryAbsolutePath)
	{
		return false;
	}

	return openOutputFileStream(binaryAbsolutePath, outFileStream, true, truncate);
}

OutputFileStream DiskLoaderModule::openBinaryAssetOutputFileStream(
	const string& assetPath,
	const bool truncate) const
{
	OutputFileStream fileStream = {};
	const bool openedFileStream = openBinaryAssetOutputFileStream(assetPath, fileStream, truncate);
	assert(openedFileStream && "[DiskLoaderModule][Assert] reason=binary_asset_output_file_open_failed");
	return fileStream;
}

bool DiskLoaderModule::ensureParentDirectory(const string& filePath) const
{
	const filesystem_path parentPath = filesystem_path(filePath).parent_path();
	if (parentPath.empty())
	{
		return true;
	}

	error_code createDirectoryError;
	create_directories(parentPath, createDirectoryError);
	return !createDirectoryError;
}

bool DiskLoaderModule::TEMP_resolveSolutionRootPath(string& outSolutionRootPath) const
{
	outSolutionRootPath.clear();

	string resourcesRootPath = {};
	if (!frameworkFileSystemResolveResourcesRootPath(resourcesRootPath))
	{
		return false;
	}

	const filesystem_path solutionRootPath = filesystem_path(resourcesRootPath).parent_path().parent_path();
	error_code solutionRootErrorCode;
	if (solutionRootPath.empty()
		|| !exists(solutionRootPath, solutionRootErrorCode)
		|| !is_directory(solutionRootPath, solutionRootErrorCode))
	{
		return false;
	}

	outSolutionRootPath = solutionRootPath.lexically_normal().string();
	return true;
}

bool DiskLoaderModule::TEMP_resolveImGuiIniFilePath(string& outIniFilePath) const
{
	outIniFilePath.clear();

	string solutionRootPath = {};
	if (!TEMP_resolveSolutionRootPath(solutionRootPath))
	{
		return false;
	}

	outIniFilePath = (filesystem_path(solutionRootPath) / "imgui.ini")
		.lexically_normal()
		.string();
	return true;
}

string DiskLoaderModule::resolveAssetPath(
	const string& path,
	const AssetFileType assetFileType) const
{
	assert(!path.empty() && "[DiskLoaderModule][Assert] reason=asset_path_missing");

	filesystem_path assetPath(path);
	assetPath.replace_extension(assetFileType == AssetFileType::document ? ".deasset" : ".de");
	return assetPath.lexically_normal().string();
}

bool DiskLoaderModule::resolveAbsolutePathFromResources(const string& pathText, string& outAbsolutePath) const
{
	outAbsolutePath.clear();
	if (pathText.empty())
	{
		return false;
	}

	const filesystem_path inputPath(pathText);
	error_code pathErrorCode;
	if (inputPath.is_absolute())
	{
		outAbsolutePath = inputPath.lexically_normal().string();
		return true;
	}

	string resourcesRootPath = {};
	if (!frameworkFileSystemResolveResourcesRootPath(resourcesRootPath))
	{
		return false;
	}

	const filesystem_path resourcesRoot(resourcesRootPath);
	outAbsolutePath = (resourcesRoot / inputPath).lexically_normal().string();
	return true;
}

string DiskLoaderModule::resolveAbsolutePathFromResources(const string& pathText) const
{
	string absolutePath = {};
	const bool validAbsolutePath = resolveAbsolutePathFromResources(pathText, absolutePath);
	assert(validAbsolutePath && "[DiskLoaderModule][Assert] reason=absolute_path_resolve_failed");
	return absolutePath;
}

bool DiskLoaderModule::resolvePathFromResources(const string& pathText, string& outAbsolutePath) const
{
	outAbsolutePath.clear();
	if (pathText.empty())
	{
		return false;
	}

	const filesystem_path inputPath(pathText);
	if (inputPath.is_absolute())
	{
		error_code pathErrorCode;
		if (!exists(inputPath, pathErrorCode))
		{
			return false;
		}

		outAbsolutePath = inputPath.lexically_normal().string();
		return true;
	}

	string resourcesRootPath = {};
	if (!frameworkFileSystemResolveResourcesRootPath(resourcesRootPath))
	{
		return false;
	}

	error_code pathErrorCode;
	const filesystem_path resourcesRoot(resourcesRootPath);
	const filesystem_path resourcesRelativePath = resourcesRoot / inputPath;
	if (exists(resourcesRelativePath, pathErrorCode))
	{
		outAbsolutePath = resourcesRelativePath.lexically_normal().string();
		return true;
	}

	const filesystem_path engineRelativePath = resourcesRoot.parent_path() / inputPath;
	if (exists(engineRelativePath, pathErrorCode))
	{
		outAbsolutePath = engineRelativePath.lexically_normal().string();
		return true;
	}

	return false;
}

string DiskLoaderModule::resolvePathFromResources(const string& pathText) const
{
	string absolutePath = {};
	const bool validAbsolutePath = resolvePathFromResources(pathText, absolutePath);
	assert(validAbsolutePath && "[DiskLoaderModule][Assert] reason=resource_path_resolve_failed");
	return absolutePath;
}

bool DiskLoaderModule::TEMP_loadRuntimeWindowResolution(uint32& outClientWidth, uint32& outClientHeight) const
{
	outClientWidth = 0;
	outClientHeight = 0;

	string runtimeIniFilePath = {};
	if (!TEMP_resolveDiskLoaderRuntimeIniFilePath(runtimeIniFilePath))
	{
		return false;
	}

	InputFileStream fileStream = {};
	if (!openInputFileStream(runtimeIniFilePath, fileStream, false))
	{
		return false;
	}

	bool widthFound = false;
	bool heightFound = false;
	string lineText = {};
	while (getline(fileStream, lineText))
	{
		uint32 parsedValue = 0;
		if (!widthFound
			&& TEMP_tryParseDiskLoaderIniUnsignedInteger(lineText, "ClientWidth", parsedValue))
		{
			outClientWidth = parsedValue;
			widthFound = true;
			continue;
		}

		if (!heightFound
			&& TEMP_tryParseDiskLoaderIniUnsignedInteger(lineText, "ClientHeight", parsedValue))
		{
			outClientHeight = parsedValue;
			heightFound = true;
		}
	}

	return widthFound
		&& heightFound
		&& outClientWidth > 0
		&& outClientHeight > 0;
}

bool DiskLoaderModule::TEMP_saveRuntimeWindowResolution(const uint32 clientWidth, const uint32 clientHeight) const
{
	if (clientWidth == 0 || clientHeight == 0)
	{
		return false;
	}

	string runtimeIniFilePath = {};
	if (!TEMP_resolveDiskLoaderRuntimeIniFilePath(runtimeIniFilePath))
	{
		return false;
	}

	OutputFileStream fileStream = {};
	if (!openOutputFileStream(runtimeIniFilePath, fileStream, false, true))
	{
		return false;
	}

	fileStream << "[Window]" << lineBreak;
	fileStream << "ClientWidth=" << clientWidth << lineBreak;
	fileStream << "ClientHeight=" << clientHeight << lineBreak;
	return fileStream.good();
}

bool DiskLoaderModule::loadBinaryFile(const string& absolutePath, vector<char>& outBinaryData) const
{
	outBinaryData.clear();
	if (absolutePath.empty())
	{
		return false;
	}

	InputFileStream fileStream = {};
	if (!openInputFileStream(absolutePath, fileStream, true))
	{
		return false;
	}

	fileStream.seekg(0, InputFileStream::end);
	const stream_position fileSize = fileStream.tellg();
	if (fileSize <= 0)
	{
		return false;
	}

	outBinaryData.resize(static_cast<size_t>(fileSize));
	fileStream.seekg(0, InputFileStream::beg);
	fileStream.read(outBinaryData.data(), fileSize);
	return fileStream.good();
}

bool DiskLoaderModule::saveBinaryFile(const string& absolutePath, const vector<char>& binaryData) const
{
	OutputFileStream fileStream = {};
	if (!openOutputFileStream(absolutePath, fileStream, true, true))
	{
		return false;
	}

	if (!binaryData.empty())
	{
		fileStream.write(binaryData.data(), static_cast<std::streamsize>(binaryData.size()));
	}

	return fileStream.good();
}
