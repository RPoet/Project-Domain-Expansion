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
	if (absolutePath.empty() || !frameworkFileSystemEnsureParentDirectory(absolutePath))
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
