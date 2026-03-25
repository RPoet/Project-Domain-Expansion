#include "Engine/Module/Shader/ShaderModule.h"

#include "Engine/Framework/Framework.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Render/Backends/Dx12/Dx12Shader.h"

static ShaderTargetPlatform getShaderTargetPlatformFromRenderBackendType(const RenderBackendType renderBackendType)
{
	switch (renderBackendType)
	{
	case RenderBackendType::dx12:
		return ShaderTargetPlatform::dx12;
	case RenderBackendType::vulkan:
		return ShaderTargetPlatform::vulkan;
	case RenderBackendType::metal:
		return ShaderTargetPlatform::metal;
	default:
		return ShaderTargetPlatform::unknown;
	}
}

static shared_pointer<ShaderObject> createShaderObjectForPlatform(
	const shared_pointer<ShaderAsset>& shaderAsset,
	const ShaderBinaryLoadRequest& binaryLoadRequest,
	vector<char>&& shaderByteCode)
{
	if (binaryLoadRequest.targetPlatform == ShaderTargetPlatform::dx12)
	{
		shared_pointer<Dx12ShaderObject> dx12ShaderObject(new Dx12ShaderObject());
		if (dx12ShaderObject == nullptr
			|| !dx12ShaderObject->initialize(shaderAsset, binaryLoadRequest, moveValue(shaderByteCode)))
		{
			return nullptr;
		}

		return dx12ShaderObject;
	}

	return nullptr;
}

bool ShaderModule::init(Framework& framework)
{
	clear();
	activeTargetPlatform = getShaderTargetPlatformFromRenderBackendType(framework.getBackendOptions().backendType);
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

shared_pointer<ShaderHandle> ShaderModule::getOrLoadShader(
	const ShaderLoadRequest& loadRequest,
	const ShaderBinaryLoadRequest& binaryLoadRequest)
{
	const ShaderBinaryLoadRequest normalizedBinaryLoadRequest = normalizeBinaryLoadRequest(binaryLoadRequest);
	const bool validLoadRequest = validateLoadRequest(loadRequest);
	const bool validBinaryLoadRequest = validateBinaryLoadRequest(normalizedBinaryLoadRequest);
	assert(validLoadRequest && validBinaryLoadRequest);

	const string cacheKey = buildShaderCacheKey(loadRequest, normalizedBinaryLoadRequest);
	const auto foundShader = shaderCache.find(cacheKey);
	if (foundShader != shaderCache.end())
	{
		return foundShader->second;
	}

	shared_pointer<ShaderHandle> shaderHandle(new ShaderHandle());
	shaderHandle->loadRequest = loadRequest;
	shaderHandle->binaryLoadRequest = normalizedBinaryLoadRequest;
	shaderHandle->cacheKey = cacheKey;
	shaderHandle->state = ShaderHandleState::pending;

	const bool targetPlatformMatched = normalizedBinaryLoadRequest.targetPlatform == activeTargetPlatform;
	assert(targetPlatformMatched && "[ShaderModule][Assert] reason=shader_target_platform_mismatch");

	string shaderBinaryAbsolutePath = {};
	const bool resolvedBinaryAbsolutePath =
		resolveShaderBinaryAbsolutePath(normalizedBinaryLoadRequest.binaryRelativePath, shaderBinaryAbsolutePath);
	assert(resolvedBinaryAbsolutePath && "[ShaderModule][Assert] reason=shader_binary_path_resolve_failed");

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[ShaderModule][Assert] reason=disk_loader_module_missing");

	vector<char> shaderByteCode = {};
	const bool loadedShaderByteCode = diskLoaderModule->loadBinaryFile(shaderBinaryAbsolutePath, shaderByteCode);
	assert(loadedShaderByteCode && "[ShaderModule][Assert] reason=shader_binary_load_failed");

	shared_pointer<ShaderAsset> shaderAsset(new ShaderAsset());
	const bool initializedShaderAsset = shaderAsset != nullptr && shaderAsset->initialize(loadRequest);
	assert(initializedShaderAsset && "[ShaderModule][Assert] reason=shader_asset_initialize_failed");

	shaderHandle->shader =
		createShaderObjectForPlatform(shaderAsset, normalizedBinaryLoadRequest, moveValue(shaderByteCode));
	const bool createdShaderObject = shaderHandle->shader != nullptr;
	assert(createdShaderObject && "[ShaderModule][Assert] reason=shader_object_create_failed");

	shaderHandle->state = ShaderHandleState::ready;
	shaderCache.emplace(cacheKey, shaderHandle);
	output << "[ShaderModule][Ready] stage=" << getShaderStageText(loadRequest.stage)
		   << " source=" << loadRequest.sourceRelativePath
		   << " binary=" << normalizedBinaryLoadRequest.binaryRelativePath
		   << " target=" << getShaderTargetPlatformText(normalizedBinaryLoadRequest.targetPlatform)
		   << " dataHash=" << shaderHandle->shader->getShaderDataHash() << lineBreak;
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

ShaderBinaryLoadRequest ShaderModule::normalizeBinaryLoadRequest(const ShaderBinaryLoadRequest& binaryLoadRequest) const
{
	ShaderBinaryLoadRequest normalizedBinaryLoadRequest = binaryLoadRequest;
	if (normalizedBinaryLoadRequest.targetPlatform == ShaderTargetPlatform::unknown)
	{
		normalizedBinaryLoadRequest.targetPlatform = activeTargetPlatform;
	}

	return normalizedBinaryLoadRequest;
}

string ShaderModule::buildShaderCacheKey(
	const ShaderLoadRequest& loadRequest,
	const ShaderBinaryLoadRequest& binaryLoadRequest) const
{
	return loadRequest.sourceRelativePath
		+ "|stage=" + std::to_string(static_cast<uint32>(loadRequest.stage))
		+ "|entry=" + loadRequest.entryPoint
		+ "|definesHash=" + std::to_string(loadRequest.definesHash)
		+ "|target=" + std::to_string(static_cast<uint32>(binaryLoadRequest.targetPlatform))
		+ "|binary=" + binaryLoadRequest.binaryRelativePath
		+ "|profile=" + binaryLoadRequest.profile;
}

bool ShaderModule::validateLoadRequest(const ShaderLoadRequest& loadRequest) const
{
	return getShaderStageIndex(loadRequest.stage) != uint32MaxValue
		&& !loadRequest.sourceRelativePath.empty()
		&& !loadRequest.entryPoint.empty();
}

bool ShaderModule::validateBinaryLoadRequest(const ShaderBinaryLoadRequest& binaryLoadRequest) const
{
	return getShaderTargetPlatformIndex(binaryLoadRequest.targetPlatform) != uint32MaxValue
		&& !binaryLoadRequest.binaryRelativePath.empty();
}

bool ShaderModule::resolveShaderBinaryAbsolutePath(
	const string& binaryRelativePath,
	string& outAbsolutePath) const
{
	outAbsolutePath.clear();
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[ShaderModule][Assert] reason=disk_loader_module_missing");
	return diskLoaderModule->resolvePathFromResources(binaryRelativePath, outAbsolutePath);
}
