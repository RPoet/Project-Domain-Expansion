#include "Engine/Assets/MaterialAsset.h"

#include "Engine/Common/FileStream.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/ShaderCompiler/ShaderCompiler.h"
#include "Engine/Module/ShaderPackage/ShaderPackageModule.h"
#include "Render/Shader.h"

#include <cstring>
#include <sstream>

static bool replaceMaterialShaderInjectionToken(
	string& inOutShaderSourceText,
	const char* tokenText,
	const string& injectedCode)
{
	assert(tokenText != nullptr && "[MaterialAsset][Assert] reason=shader_injection_token_missing");
	if (injectedCode.empty())
	{
		return true;
	}

	const size_t tokenIndex = inOutShaderSourceText.find(tokenText);
	assert(tokenIndex != string::npos && "[MaterialAsset][Assert] reason=shader_injection_token_not_found");
	if (tokenIndex == string::npos)
	{
		return false;
	}

	inOutShaderSourceText.replace(tokenIndex, strlen(tokenText), injectedCode);
	return true;
}

static void appendMaterialShaderHashText(uint64& inOutHashValue, const string& text)
{
	inOutHashValue = platformHashCombine(inOutHashValue, static_cast<uint64>(text.size()));
	for (size_t characterIndex = 0; characterIndex < text.size(); ++characterIndex)
	{
		inOutHashValue = platformHashCombine(inOutHashValue, static_cast<uint64>(static_cast<unsigned char>(text[characterIndex])));
	}
}

static uint64 computeMaterialShaderCacheHash(
	const string& shaderSourceText,
	const ShaderTargetPlatform targetPlatform,
	const ShaderPackageVariant& shaderVariant)
{
	uint64 hashValue = platformHashCombine(platformHashOffsetBasis, static_cast<uint64>(targetPlatform));
	appendMaterialShaderHashText(hashValue, shaderSourceText);

	const ShaderStage shaderStages[2] = { ShaderStage::vertex, ShaderStage::pixel };
	for (uint32 shaderStageArrayIndex = 0; shaderStageArrayIndex < 2; ++shaderStageArrayIndex)
	{
		const ShaderStage shaderStage = shaderStages[shaderStageArrayIndex];
		const ShaderLoadRequest& shaderLoadRequest = shaderVariant.getLoadRequest(shaderStage);
		const ShaderBinaryLoadRequest& shaderBinaryLoadRequest = shaderVariant.getBinaryLoadRequest(shaderStage);
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(shaderStage));
		appendMaterialShaderHashText(hashValue, shaderLoadRequest.entryPoint);
		hashValue = platformHashCombine(hashValue, shaderLoadRequest.definesHash);
		appendMaterialShaderHashText(hashValue, shaderBinaryLoadRequest.profile);
	}

	return hashValue;
}

static bool buildMaterialShaderCompileRequest(
	const MaterialAsset& materialAsset,
	const ShaderPackageVariant& shaderVariant,
	const ShaderStage shaderStage,
	const ShaderTargetPlatform targetPlatform,
	ShaderCompileRequest& outCompileRequest)
{
	outCompileRequest = {};
	const ShaderLoadRequest& shaderLoadRequest = shaderVariant.getLoadRequest(shaderStage);
	const ShaderBinaryLoadRequest& shaderBinaryLoadRequest = shaderVariant.getBinaryLoadRequest(shaderStage);
	const bool hasEntryPoint = !shaderLoadRequest.entryPoint.empty();
	const bool hasProfile = !shaderBinaryLoadRequest.profile.empty();
	const bool validVariantCompileMetadata =
		shaderLoadRequest.stage == shaderStage && shaderBinaryLoadRequest.targetPlatform == targetPlatform && hasEntryPoint && hasProfile;
	assert(validVariantCompileMetadata && "[MaterialAsset][Assert] reason=variant_compile_metadata_missing");
	if (!validVariantCompileMetadata)
	{
		return false;
	}

	outCompileRequest.stage = shaderStage;
	outCompileRequest.sourceRelativePath = materialAsset.getShaderTemplatePath();
	outCompileRequest.entryPoint = shaderLoadRequest.entryPoint;
	outCompileRequest.definesHash = shaderLoadRequest.definesHash;
	outCompileRequest.targetPlatform = targetPlatform;
	outCompileRequest.profile = shaderBinaryLoadRequest.profile;
	return true;
}

const string& MaterialAsset::getShaderTemplatePath() const
{
	return shaderTemplatePath;
}

const string& MaterialAsset::getShaderPackagePath() const
{
	return shaderPackagePath;
}

const string& MaterialAsset::getShaderVariantName() const
{
	return shaderVariantName;
}

const string& MaterialAsset::getVertexShaderInjectedCode() const
{
	return vertexShaderInjectedCode;
}

const string& MaterialAsset::getPixelShaderInjectedCode() const
{
	return pixelShaderInjectedCode;
}

bool MaterialAsset::hasShaderEdits() const
{
	return !vertexShaderInjectedCode.empty() || !pixelShaderInjectedCode.empty();
}

bool MaterialAsset::buildShaderSourceText(string& outShaderSourceText) const
{
	outShaderSourceText.clear();
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[MaterialAsset][Assert] reason=disk_loader_module_missing");

	string shaderTemplateAbsolutePath = {};
	if (!diskLoaderModule->resolvePathFromResources(shaderTemplatePath, shaderTemplateAbsolutePath))
	{
		return false;
	}

	InputFileStream fileStream = diskLoaderModule->openInputFileStream(shaderTemplateAbsolutePath, false);
	if (!fileStream.is_open())
	{
		return false;
	}

	string_output_stream shaderSourceStream = {};
	shaderSourceStream << fileStream.rdbuf();
	outShaderSourceText = shaderSourceStream.str();
	if (outShaderSourceText.empty())
	{
		return false;
	}

	return replaceMaterialShaderInjectionToken(outShaderSourceText, getVertexShaderInjectionToken(), vertexShaderInjectedCode)
		&& replaceMaterialShaderInjectionToken(outShaderSourceText, getPixelShaderInjectionToken(), pixelShaderInjectedCode);
}

bool MaterialAsset::getOrCompileEditedShaders(
	const ShaderTargetPlatform targetPlatform,
	const ShaderPackageVariant& shaderVariant,
	shared_pointer<ShaderObject>& outVertexShader,
	shared_pointer<ShaderObject>& outPixelShader) const
{
	outVertexShader = nullptr;
	outPixelShader = nullptr;
	if (!hasShaderEdits() || getShaderTargetPlatformIndex(targetPlatform) == uint32MaxValue)
	{
		return false;
	}

	string shaderSourceText = {};
	if (!buildShaderSourceText(shaderSourceText))
	{
		return false;
	}

	const uint64 shaderSourceHash = computeMaterialShaderCacheHash(shaderSourceText, targetPlatform, shaderVariant);
	if (runtimeShaderCache.targetPlatform == targetPlatform
		&& runtimeShaderCache.shaderSourceHash == shaderSourceHash
		&& runtimeShaderCache.vertexShader != nullptr
		&& runtimeShaderCache.pixelShader != nullptr)
	{
		outVertexShader = runtimeShaderCache.vertexShader;
		outPixelShader = runtimeShaderCache.pixelShader;
		return true;
	}

	ShaderCompiler shaderCompiler = {};
	const char* shaderSource = shaderSourceText.c_str();
	ShaderCompileRequest vertexCompileRequest = {};
	if (!buildMaterialShaderCompileRequest(*this, shaderVariant, ShaderStage::vertex, targetPlatform, vertexCompileRequest))
	{
		clearRuntimeShaderCache();
		return false;
	}
	ShaderCompileResult vertexCompileResult = {};
	const bool compiledVertexShader = shaderCompiler.compileFromMemory(vertexCompileRequest, shaderSource, vertexCompileResult);
	if (!compiledVertexShader || !vertexCompileResult.success || vertexCompileResult.shaderObject == nullptr)
	{
		error << "[MaterialAsset][CompileFailed] stage=vertex asset=" << assetPath
			  << " diagnostic=" << vertexCompileResult.diagnosticText << lineBreak;
		clearRuntimeShaderCache();
		return false;
	}

	ShaderCompileRequest pixelCompileRequest = {};
	if (!buildMaterialShaderCompileRequest(*this, shaderVariant, ShaderStage::pixel, targetPlatform, pixelCompileRequest))
	{
		clearRuntimeShaderCache();
		return false;
	}
	ShaderCompileResult pixelCompileResult = {};
	const bool compiledPixelShader = shaderCompiler.compileFromMemory(pixelCompileRequest, shaderSource, pixelCompileResult);
	if (!compiledPixelShader || !pixelCompileResult.success || pixelCompileResult.shaderObject == nullptr)
	{
		error << "[MaterialAsset][CompileFailed] stage=pixel asset=" << assetPath
			  << " diagnostic=" << pixelCompileResult.diagnosticText << lineBreak;
		clearRuntimeShaderCache();
		return false;
	}

	runtimeShaderCache.targetPlatform = targetPlatform;
	runtimeShaderCache.shaderSourceHash = shaderSourceHash;
	runtimeShaderCache.vertexShader = vertexCompileResult.shaderObject;
	runtimeShaderCache.pixelShader = pixelCompileResult.shaderObject;
	outVertexShader = runtimeShaderCache.vertexShader;
	outPixelShader = runtimeShaderCache.pixelShader;
	return true;
}

bool MaterialAsset::resolveEffectiveShaders(
	ShaderPackageModule& shaderPackageModule,
	const shared_pointer<MaterialAsset>& materialAsset,
	const ShaderTargetPlatform targetPlatform,
	shared_pointer<ShaderObject>& outVertexShader,
	shared_pointer<ShaderObject>& outPixelShader)
{
	outVertexShader = nullptr;
	outPixelShader = nullptr;

	shared_pointer<ShaderPackageAsset> resolvedShaderPackage =
		shaderPackageModule.getOrLoadPackage(getDefaultShaderPackagePath());
	if (resolvedShaderPackage == nullptr || resolvedShaderPackage->state != ShaderPackageState::ready)
	{
		return false;
	}

	const ShaderPackageVariant* shaderVariant =
		shaderPackageModule.findVariantByName(*resolvedShaderPackage, getDefaultShaderVariantName());
	if (materialAsset != nullptr)
	{
		const string& shaderPackagePath = materialAsset->getShaderPackagePath();
		const string& shaderVariantName = materialAsset->getShaderVariantName();
		if (!shaderPackagePath.empty() && !shaderVariantName.empty())
		{
			shared_pointer<ShaderPackageAsset> materialShaderPackage = shaderPackageModule.getOrLoadPackage(shaderPackagePath);
			if (materialShaderPackage != nullptr && materialShaderPackage->state == ShaderPackageState::ready)
			{
				const ShaderPackageVariant* materialShaderVariant =
					shaderPackageModule.findVariantByName(*materialShaderPackage, shaderVariantName);
				if (materialShaderVariant != nullptr)
				{
					resolvedShaderPackage = materialShaderPackage;
					shaderVariant = materialShaderVariant;
				}
			}
		}
	}

	const bool hasShaderVariant = shaderVariant != nullptr;
	if (!hasShaderVariant)
	{
		return false;
	}

	if (materialAsset != nullptr)
	{
		if (materialAsset->hasShaderEdits())
		{
			return materialAsset->getOrCompileEditedShaders(targetPlatform, *shaderVariant, outVertexShader, outPixelShader);
		}
	}

	outVertexShader = shaderVariant->getShader(ShaderStage::vertex);
	outPixelShader = shaderVariant->getShader(ShaderStage::pixel);
	return outVertexShader != nullptr && outPixelShader != nullptr;
}

void MaterialAsset::setShaderTemplatePath(const string& inShaderTemplatePath)
{
	shaderTemplatePath = inShaderTemplatePath;
	clearRuntimeShaderCache();
}

void MaterialAsset::setShaderPackagePath(const string& inShaderPackagePath)
{
	shaderPackagePath = inShaderPackagePath;
	clearRuntimeShaderCache();
}

void MaterialAsset::setShaderVariantName(const string& inShaderVariantName)
{
	shaderVariantName = inShaderVariantName;
	clearRuntimeShaderCache();
}

void MaterialAsset::setVertexShaderInjectedCode(const string& inVertexShaderInjectedCode)
{
	vertexShaderInjectedCode = inVertexShaderInjectedCode;
	clearRuntimeShaderCache();
}

void MaterialAsset::setPixelShaderInjectedCode(const string& inPixelShaderInjectedCode)
{
	pixelShaderInjectedCode = inPixelShaderInjectedCode;
	clearRuntimeShaderCache();
}

const char* MaterialAsset::getDefaultShaderTemplatePath()
{
	return "Shaders/Geometry/GeometryBaseColor.hlsl";
}

const char* MaterialAsset::getDefaultShaderPackagePath()
{
	return "Shaders/Packages/GeometryBaseColor.shaderpkg";
}

const char* MaterialAsset::getDefaultShaderVariantName()
{
	return "GeometryDefault";
}

const char* MaterialAsset::getVertexShaderInjectionToken()
{
	return "/* MATERIAL_VERTEX_EDIT */";
}

const char* MaterialAsset::getPixelShaderInjectionToken()
{
	return "/* MATERIAL_PIXEL_EDIT */";
}

void MaterialAsset::clear()
{
	Asset::clear();
	shaderTemplatePath = getDefaultShaderTemplatePath();
	shaderPackagePath = getDefaultShaderPackagePath();
	shaderVariantName = getDefaultShaderVariantName();
	vertexShaderInjectedCode.clear();
	pixelShaderInjectedCode.clear();
	clearRuntimeShaderCache();
}

bool MaterialAsset::isDocumentBinaryLayoutCompatible(const XMLKeyValueDocument& document, const bool documentHasBinary) const
{
	XML& xml = XML::get();
	uint32 documentVersion = uint32MaxValue;
	const bool hasVersion = xml.readProperty(document, "deasset.version", documentVersion);
	assert(hasVersion && "[MaterialAsset][Assert] reason=document_version_missing");
	if (!hasVersion)
	{
		return false;
	}

	if (documentVersion == 1)
	{
		return !documentHasBinary;
	}

	return documentHasBinary == hasBinary;
}

void MaterialAsset::clearRuntimeShaderCache() const
{
	runtimeShaderCache.targetPlatform = static_cast<ShaderTargetPlatform>(0);
	runtimeShaderCache.shaderSourceHash = 0;
	runtimeShaderCache.vertexShader = nullptr;
	runtimeShaderCache.pixelShader = nullptr;
}

void MaterialAsset::writeAssetProperty(OutputFileStream& fileStream) const
{
	assert(hasBinary && "[MaterialAsset][Assert] reason=material_asset_binary_storage_required");
	XML& xml = XML::get();
	xml.writeProperty(fileStream, "version", version);
	xml.writeProperty(fileStream, "shaderTemplatePath", shaderTemplatePath);
	xml.writeProperty(fileStream, "shaderPackagePath", shaderPackagePath);
	xml.writeProperty(fileStream, "shaderVariantName", shaderVariantName);
}

void MaterialAsset::readAssetProperty(const XMLKeyValueDocument& document)
{
	XML& xml = XML::get();

	uint32 documentVersion = uint32MaxValue;
	const bool hasVersion = xml.readProperty(document, "deasset.version", documentVersion);
	assert(hasVersion && "[MaterialAsset][Assert] reason=document_version_missing");
	assert((documentVersion == 1 || documentVersion == version) && "[MaterialAsset][Assert] reason=document_version_mismatch");
	assert((documentVersion == 1 || hasBinary) && "[MaterialAsset][Assert] reason=document_binary_flag_missing");

	shaderTemplatePath = getDefaultShaderTemplatePath();
	shaderPackagePath = getDefaultShaderPackagePath();
	shaderVariantName = getDefaultShaderVariantName();
	vertexShaderInjectedCode.clear();
	pixelShaderInjectedCode.clear();
	clearRuntimeShaderCache();
	xml.readProperty(document, "deasset.shaderTemplatePath", shaderTemplatePath);
	xml.readProperty(document, "deasset.shaderPackagePath", shaderPackagePath);
	xml.readProperty(document, "deasset.shaderVariantName", shaderVariantName);
	if (documentVersion == 1)
	{
		xml.readProperty(document, "deasset.vertexShaderEditText", vertexShaderInjectedCode);
		xml.readProperty(document, "deasset.pixelShaderEditText", pixelShaderInjectedCode);
	}
}

void MaterialAsset::serialize(OutputFileStream& fileStream) const
{
	const uint32 vertexShaderInjectedCodeLength = static_cast<uint32>(vertexShaderInjectedCode.size());
	fileStream << vertexShaderInjectedCodeLength;
	if (vertexShaderInjectedCodeLength != 0)
	{
		fileStream.write(vertexShaderInjectedCode.data(), static_cast<stream_size>(vertexShaderInjectedCodeLength));
	}

	const uint32 pixelShaderInjectedCodeLength = static_cast<uint32>(pixelShaderInjectedCode.size());
	fileStream << pixelShaderInjectedCodeLength;
	if (pixelShaderInjectedCodeLength != 0)
	{
		fileStream.write(pixelShaderInjectedCode.data(), static_cast<stream_size>(pixelShaderInjectedCodeLength));
	}
}

void MaterialAsset::deserialize(InputFileStream& fileStream)
{
	TRACE_EVENT("asset", "MaterialAsset::deserialize");
	vertexShaderInjectedCode.clear();
	pixelShaderInjectedCode.clear();

	uint32 vertexShaderInjectedCodeLength = 0;
	fileStream >> vertexShaderInjectedCodeLength;
	if (vertexShaderInjectedCodeLength != 0)
	{
		vertexShaderInjectedCode.resize(vertexShaderInjectedCodeLength);
		fileStream.read(vertexShaderInjectedCode.data(), static_cast<stream_size>(vertexShaderInjectedCodeLength));
	}

	uint32 pixelShaderInjectedCodeLength = 0;
	fileStream >> pixelShaderInjectedCodeLength;
	if (pixelShaderInjectedCodeLength != 0)
	{
		pixelShaderInjectedCode.resize(pixelShaderInjectedCodeLength);
		fileStream.read(pixelShaderInjectedCode.data(), static_cast<stream_size>(pixelShaderInjectedCodeLength));
	}

	clearRuntimeShaderCache();
}
