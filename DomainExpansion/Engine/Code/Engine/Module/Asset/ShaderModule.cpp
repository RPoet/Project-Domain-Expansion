#include "Engine/Module/Asset/ShaderModule.h"

#include "Engine/Framework/FrameworkFileSystem.h"

#include <fstream>

static bool readBinaryFileAll(const string& filePath, vector<char>& outBinaryData)
{
	outBinaryData.clear();

	input_file_stream fileStream(filePath, input_file_stream::binary | input_file_stream::ate);
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

bool ShaderModule::init(Framework& framework)
{
	unused(framework);
	clear();
	return true;
}

void ShaderModule::preUpdate()
{
}

void ShaderModule::postUpdate()
{
}

void ShaderModule::shutdown()
{
	clear();
}

shared_pointer<ShaderAssetHandle> ShaderModule::getOrLoadShader(const ShaderLoadRequest& loadRequest)
{
	if (!validateLoadRequest(loadRequest))
	{
		error << "[ShaderModule][Error] reason=invalid_load_request"
			  << " stage=" << getShaderStageText(loadRequest.stage)
			  << " path=" << loadRequest.shaderRelativePath
			  << " entry=" << loadRequest.entryPoint
			  << " profile=" << loadRequest.profile << lineBreak;
		return nullptr;
	}

	const string cacheKey = buildShaderCacheKey(loadRequest);
	const auto foundShader = shaderCache.find(cacheKey);
	if (foundShader != shaderCache.end())
	{
		return foundShader->second;
	}

	shared_pointer<ShaderAssetHandle> shaderHandle(new ShaderAssetHandle());
	shaderHandle->loadRequest = loadRequest;
	shaderHandle->cacheKey = cacheKey;
	shaderHandle->state = ShaderAssetState::pending;
	shaderHandle->shaderAsset = shared_pointer<ShaderAsset>(new ShaderAsset());

	string shaderAbsolutePath = {};
	if (!resolveShaderAbsolutePath(loadRequest.shaderRelativePath, shaderAbsolutePath))
	{
		shaderHandle->state = ShaderAssetState::failed;
		shaderCache.emplace(cacheKey, shaderHandle);
		error << "[ShaderModule][Error] stage=" << getShaderStageText(loadRequest.stage)
			  << " path=" << loadRequest.shaderRelativePath
			  << " reason=shader_path_resolve_failed" << lineBreak;
		return shaderHandle;
	}

	if (shaderHandle->shaderAsset == nullptr
		|| !readBinaryFileAll(shaderAbsolutePath, shaderHandle->shaderAsset->byteCode))
	{
		shaderHandle->state = ShaderAssetState::failed;
		shaderCache.emplace(cacheKey, shaderHandle);
		error << "[ShaderModule][Error] stage=" << getShaderStageText(loadRequest.stage)
			  << " path=" << shaderAbsolutePath
			  << " reason=shader_binary_load_failed" << lineBreak;
		return shaderHandle;
	}

	shaderHandle->state = ShaderAssetState::ready;
	shaderCache.emplace(cacheKey, shaderHandle);
	output << "[ShaderModule][Ready] stage=" << getShaderStageText(loadRequest.stage)
		   << " path=" << loadRequest.shaderRelativePath
		   << " byteCodeSize=" << shaderHandle->shaderAsset->byteCode.size() << lineBreak;
	return shaderHandle;
}

void ShaderModule::clear()
{
	shaderCache.clear();
}

uint32 ShaderModule::getCachedShaderCount() const
{
	return static_cast<uint32>(shaderCache.size());
}

string ShaderModule::buildShaderCacheKey(const ShaderLoadRequest& loadRequest) const
{
	return loadRequest.shaderRelativePath
		+ "|stage=" + std::to_string(static_cast<uint32>(loadRequest.stage))
		+ "|entry=" + loadRequest.entryPoint
		+ "|profile=" + loadRequest.profile
		+ "|definesHash=" + std::to_string(loadRequest.definesHash);
}

bool ShaderModule::validateLoadRequest(const ShaderLoadRequest& loadRequest) const
{
	return getShaderStageIndex(loadRequest.stage) != uint32MaxValue
		&& !loadRequest.shaderRelativePath.empty()
		&& !loadRequest.entryPoint.empty()
		&& !loadRequest.profile.empty();
}

bool ShaderModule::resolveShaderAbsolutePath(
	const string& shaderRelativePath,
	string& outAbsolutePath) const
{
	outAbsolutePath.clear();
	return frameworkFileSystemResolvePathFromResources(shaderRelativePath, outAbsolutePath);
}
