#include "Engine/Module/Asset/DiskLoaderModule.h"

#include "Engine/Framework/FrameworkFileSystem.h"

#include <fstream>

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

bool DiskLoaderModule::resolvePathFromResources(const string& pathText, string& outAbsolutePath) const
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

bool DiskLoaderModule::loadBinaryFile(const string& absolutePath, vector<char>& outBinaryData) const
{
	outBinaryData.clear();
	if (absolutePath.empty())
	{
		return false;
	}

	input_file_stream fileStream(absolutePath, input_file_stream::binary | input_file_stream::ate);
	if (!fileStream.is_open())
	{
		return false;
	}

	const stream_position fileSize = fileStream.tellg();
	if (fileSize <= 0)
	{
		return false;
	}

	outBinaryData.resize(static_cast<size_t>(fileSize));
	fileStream.seekg(0, std::ios::beg);
	fileStream.read(outBinaryData.data(), fileSize);
	return fileStream.good();
}

bool DiskLoaderModule::saveBinaryFile(const string& absolutePath, const vector<char>& binaryData) const
{
	if (absolutePath.empty() || !ensureParentDirectory(absolutePath))
	{
		return false;
	}

	output_file_stream fileStream(absolutePath, output_file_stream::binary | output_file_stream::trunc);
	if (!fileStream.is_open())
	{
		return false;
	}

	if (!binaryData.empty())
	{
		fileStream.write(binaryData.data(), static_cast<std::streamsize>(binaryData.size()));
	}

	return fileStream.good();
}
