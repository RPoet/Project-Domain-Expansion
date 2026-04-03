#include "Engine/Module/ShaderPackage/ShaderPackageModule.h"

#include "Engine/Module/DiskLoader/DiskLoaderModule.h"

#include <fstream>
#include <sstream>

enum class ShaderPackageParseSection : uint32
{
	none = 0,
	package = 1,
	shader = 2,
	variant = 3,
};

struct ShaderPackageShaderRecord
{
	string id = {};
	ShaderLoadRequest loadRequest = {};
	ShaderBinaryLoadRequest binaryLoadRequest = {};
};

struct ShaderPackageVariantRecord
{
	string name = {};
	string shaderIds[shaderStageCount] = {};
};

static string trimShaderPackageText(const string& text)
{
	size_t beginIndex = 0;
	while (beginIndex < text.length())
	{
		const char character = text[beginIndex];
		if (character != ' ' && character != '\t' && character != '\r' && character != '\n')
		{
			break;
		}

		++beginIndex;
	}

	size_t endIndex = text.length();
	while (endIndex > beginIndex)
	{
		const char character = text[endIndex - 1];
		if (character != ' ' && character != '\t' && character != '\r' && character != '\n')
		{
			break;
		}

		--endIndex;
	}

	return text.substr(beginIndex, endIndex - beginIndex);
}

static bool parseShaderStageText(const string& text, ShaderStage& outShaderStage)
{
	if (text == "vertex")
	{
		outShaderStage = ShaderStage::vertex;
		return true;
	}

	if (text == "pixel")
	{
		outShaderStage = ShaderStage::pixel;
		return true;
	}

	if (text == "compute")
	{
		outShaderStage = ShaderStage::compute;
		return true;
	}

	outShaderStage = ShaderStage::unknown;
	return false;
}

static string getDefaultShaderBinaryProfileText(
	const ShaderStage shaderStage,
	const ShaderTargetPlatform shaderTargetPlatform)
{
	if (shaderTargetPlatform != ShaderTargetPlatform::dx12)
	{
		return {};
	}

	switch (shaderStage)
	{
	case ShaderStage::vertex:
		return "vs_6_9";
	case ShaderStage::pixel:
		return "ps_6_9";
	case ShaderStage::compute:
		return "cs_6_9";
	default:
		return {};
	}
}

static bool parseUint64Text(const string& text, uint64& outValue)
{
	string_input_stream parser(text);
	uint64 parsedValue = 0;
	parser >> parsedValue;
	if (!parser || !parser.eof())
	{
		return false;
	}

	outValue = parsedValue;
	return true;
}

static bool verifyShaderPackageCondition(const bool condition, const char* assertReason)
{
	assert(condition && assertReason);
	return condition;
}

static bool flushShaderRecord(
	const string& packageAbsolutePath,
	const uint32 lineNumber,
	ShaderPackageShaderRecord& shaderRecord,
	vector<ShaderPackageShaderRecord>& outShaderRecords)
{
	unused(packageAbsolutePath);
	unused(lineNumber);
	if (!verifyShaderPackageCondition(!shaderRecord.id.empty(), "[ShaderPackageModule][Assert] reason=shader_id_missing"))
	{
		return false;
	}

	if (!verifyShaderPackageCondition(
		getShaderStageIndex(shaderRecord.loadRequest.stage) != uint32MaxValue,
		"[ShaderPackageModule][Assert] reason=shader_stage_invalid"))
	{
		return false;
	}

	if (!verifyShaderPackageCondition(
		!shaderRecord.binaryLoadRequest.binaryRelativePath.empty(),
		"[ShaderPackageModule][Assert] reason=shader_binary_missing"))
	{
		return false;
	}

	if (shaderRecord.loadRequest.entryPoint.empty())
	{
		shaderRecord.loadRequest.entryPoint = "main";
	}

	if (shaderRecord.loadRequest.sourceRelativePath.empty())
	{
		shaderRecord.loadRequest.sourceRelativePath = shaderRecord.binaryLoadRequest.binaryRelativePath;
	}

	if (shaderRecord.binaryLoadRequest.profile.empty())
	{
		shaderRecord.binaryLoadRequest.profile = getDefaultShaderBinaryProfileText(
			shaderRecord.loadRequest.stage,
			shaderRecord.binaryLoadRequest.targetPlatform);
	}

	outShaderRecords.push_back(shaderRecord);
	shaderRecord = {};
	return true;
}

static bool flushVariantRecord(
	const string& packageAbsolutePath,
	const uint32 lineNumber,
	ShaderPackageVariantRecord& variantRecord,
	vector<ShaderPackageVariantRecord>& outVariantRecords)
{
	unused(packageAbsolutePath);
	unused(lineNumber);
	if (!verifyShaderPackageCondition(!variantRecord.name.empty(), "[ShaderPackageModule][Assert] reason=variant_name_missing"))
	{
		return false;
	}

	bool hasAnyShader = false;
	for (uint32 shaderStageIndex = 1; shaderStageIndex < shaderStageCount; ++shaderStageIndex)
	{
		if (!variantRecord.shaderIds[shaderStageIndex].empty())
		{
			hasAnyShader = true;
			break;
		}
	}

	if (!verifyShaderPackageCondition(hasAnyShader, "[ShaderPackageModule][Assert] reason=variant_shader_missing"))
	{
		return false;
	}

	outVariantRecords.push_back(variantRecord);
	variantRecord = {};
	return true;
}

static bool parseShaderPackageManifest(
	const string& packageAbsolutePath,
	vector<ShaderPackageShaderRecord>& outShaderRecords,
	vector<ShaderPackageVariantRecord>& outVariantRecords)
{
	outShaderRecords.clear();
	outVariantRecords.clear();

	input_file_stream fileStream(packageAbsolutePath);
	if (!verifyShaderPackageCondition(fileStream.is_open(), "[ShaderPackageModule][Assert] reason=package_open_failed"))
	{
		return false;
	}

	ShaderPackageParseSection parseSection = ShaderPackageParseSection::none;
	ShaderPackageShaderRecord shaderRecord = {};
	ShaderPackageVariantRecord variantRecord = {};
	bool hasActiveShaderRecord = false;
	bool hasActiveVariantRecord = false;

	string lineText = {};
	uint32 lineNumber = 0;
	while (std::getline(fileStream, lineText))
	{
		++lineNumber;
		if (lineNumber == 1
			&& lineText.size() >= 3
			&& static_cast<unsigned char>(lineText[0]) == 0xEF
			&& static_cast<unsigned char>(lineText[1]) == 0xBB
			&& static_cast<unsigned char>(lineText[2]) == 0xBF)
		{
			lineText = lineText.substr(3);
		}

		const size_t commentIndex = lineText.find('#');
		if (commentIndex != string::npos)
		{
			lineText = lineText.substr(0, commentIndex);
		}

		lineText = trimShaderPackageText(lineText);
		if (lineText.empty())
		{
			continue;
		}

		if (lineText.front() == '[' && lineText.back() == ']')
		{
			if (hasActiveShaderRecord && !flushShaderRecord(packageAbsolutePath, lineNumber, shaderRecord, outShaderRecords))
			{
				return false;
			}

			if (hasActiveVariantRecord && !flushVariantRecord(packageAbsolutePath, lineNumber, variantRecord, outVariantRecords))
			{
				return false;
			}

			hasActiveShaderRecord = false;
			hasActiveVariantRecord = false;
			parseSection = ShaderPackageParseSection::none;

			const string sectionName = trimShaderPackageText(lineText.substr(1, lineText.length() - 2));
			if (sectionName == "Package")
			{
				parseSection = ShaderPackageParseSection::package;
			}
			else if (sectionName == "Shader")
			{
				parseSection = ShaderPackageParseSection::shader;
				hasActiveShaderRecord = true;
			}
			else if (sectionName == "Variant")
			{
				parseSection = ShaderPackageParseSection::variant;
				hasActiveVariantRecord = true;
			}

			continue;
		}

		const size_t delimiterIndex = lineText.find('=');
		if (!verifyShaderPackageCondition(delimiterIndex != string::npos, "[ShaderPackageModule][Assert] reason=invalid_key_value"))
		{
			return false;
		}

		const string key = trimShaderPackageText(lineText.substr(0, delimiterIndex));
		const string value = trimShaderPackageText(lineText.substr(delimiterIndex + 1));
		if (parseSection == ShaderPackageParseSection::shader && hasActiveShaderRecord)
		{
			if (key == "id")
			{
				shaderRecord.id = value;
				continue;
			}

			if (key == "stage")
			{
				if (!verifyShaderPackageCondition(
					parseShaderStageText(value, shaderRecord.loadRequest.stage),
					"[ShaderPackageModule][Assert] reason=shader_stage_parse_failed"))
				{
					return false;
				}
				continue;
			}

			if (key == "file")
			{
				shaderRecord.binaryLoadRequest.binaryRelativePath = value;
				continue;
			}

			if (key == "source")
			{
				shaderRecord.loadRequest.sourceRelativePath = value;
				continue;
			}

			if (key == "entry")
			{
				shaderRecord.loadRequest.entryPoint = value;
				continue;
			}

			if (key == "profile")
			{
				shaderRecord.binaryLoadRequest.profile = value;
				continue;
			}

			if (key == "definesHash")
			{
				if (!verifyShaderPackageCondition(
					parseUint64Text(value, shaderRecord.loadRequest.definesHash),
					"[ShaderPackageModule][Assert] reason=defines_hash_parse_failed"))
				{
					return false;
				}
				continue;
			}

			continue;
		}

		if (parseSection == ShaderPackageParseSection::variant && hasActiveVariantRecord)
		{
			if (key == "name")
			{
				variantRecord.name = value;
				continue;
			}

			ShaderStage shaderStage = ShaderStage::unknown;
			if (parseShaderStageText(key, shaderStage))
			{
				const uint32 shaderStageIndex = getShaderStageIndex(shaderStage);
				if (shaderStageIndex != uint32MaxValue)
				{
					variantRecord.shaderIds[shaderStageIndex] = value;
				}
			}
		}
	}

	if (hasActiveShaderRecord && !flushShaderRecord(packageAbsolutePath, lineNumber, shaderRecord, outShaderRecords))
	{
		return false;
	}

	if (hasActiveVariantRecord && !flushVariantRecord(packageAbsolutePath, lineNumber, variantRecord, outVariantRecords))
	{
		return false;
	}

	if (!verifyShaderPackageCondition(!outShaderRecords.empty(), "[ShaderPackageModule][Assert] reason=shader_section_missing"))
	{
		return false;
	}

	if (!verifyShaderPackageCondition(!outVariantRecords.empty(), "[ShaderPackageModule][Assert] reason=variant_section_missing"))
	{
		return false;
	}

	return true;
}

static bool resolveShaderRecords(
	const string& packageRelativePath,
	const vector<ShaderPackageShaderRecord>& shaderRecords,
	unordered_map<string, shared_pointer<ShaderHandle>>& outShaderById)
{
	outShaderById.clear();
	shared_pointer<ShaderModule> shaderModule = ShaderModule::get();
	unused(packageRelativePath);
	if (!verifyShaderPackageCondition(shaderModule != nullptr, "[ShaderPackageModule][Assert] reason=shader_module_missing"))
	{
		return false;
	}

	for (uint32 shaderIndex = 0; shaderIndex < static_cast<uint32>(shaderRecords.size()); ++shaderIndex)
	{
		const ShaderPackageShaderRecord& shaderRecord = shaderRecords[shaderIndex];
		if (!verifyShaderPackageCondition(!shaderRecord.id.empty(), "[ShaderPackageModule][Assert] reason=shader_id_empty"))
		{
			return false;
		}

		if (!verifyShaderPackageCondition(
			outShaderById.find(shaderRecord.id) == outShaderById.end(),
			"[ShaderPackageModule][Assert] reason=shader_id_duplicate"))
		{
			return false;
		}

		shared_pointer<ShaderHandle> shaderHandle =
			shaderModule->getOrLoadShader(shaderRecord.loadRequest, shaderRecord.binaryLoadRequest);
		if (!verifyShaderPackageCondition(
			shaderHandle != nullptr && shaderHandle->state == ShaderHandleState::ready,
			"[ShaderPackageModule][Assert] reason=shader_load_failed"))
		{
			return false;
		}

		outShaderById.emplace(shaderRecord.id, shaderHandle);
	}

	return true;
}

static bool buildVariants(
	const string& packageRelativePath,
	const vector<ShaderPackageVariantRecord>& variantRecords,
	const unordered_map<string, shared_pointer<ShaderHandle>>& shaderById,
	vector<ShaderPackageVariant>& outVariants)
{
	outVariants.clear();
	outVariants.reserve(variantRecords.size());

	for (uint32 variantIndex = 0; variantIndex < static_cast<uint32>(variantRecords.size()); ++variantIndex)
	{
		const ShaderPackageVariantRecord& variantRecord = variantRecords[variantIndex];
		ShaderPackageVariant variant = {};
		variant.name = variantRecord.name;

		uint32 linkedShaderCount = 0;
		for (uint32 shaderStageIndex = 1; shaderStageIndex < shaderStageCount; ++shaderStageIndex)
		{
			const string& shaderId = variantRecord.shaderIds[shaderStageIndex];
			if (shaderId.empty())
			{
				continue;
			}

			const auto foundShader = shaderById.find(shaderId);
			if (!verifyShaderPackageCondition(
				foundShader != shaderById.end()
				&& foundShader->second != nullptr
				&& foundShader->second->shader != nullptr,
				"[ShaderPackageModule][Assert] reason=variant_shader_id_missing"))
			{
				return false;
			}

			variant.shaders[shaderStageIndex] = foundShader->second->shader;
			++linkedShaderCount;
		}

		if (!verifyShaderPackageCondition(linkedShaderCount != 0, "[ShaderPackageModule][Assert] reason=variant_shader_link_empty"))
		{
			return false;
		}

		outVariants.push_back(moveValue(variant));
	}

	return true;
}

bool ShaderPackageModule::init(Framework& framework)
{
	unused(framework);
	clear();
	return true;
}

void ShaderPackageModule::preUpdate()
{
}

void ShaderPackageModule::postUpdate()
{
}

void ShaderPackageModule::shutdown()
{
	clear();
}

shared_pointer<ShaderPackageAsset> ShaderPackageModule::getOrLoadPackage(const string& packageRelativePath)
{
	if (!verifyShaderPackageCondition(!packageRelativePath.empty(), "[ShaderPackageModule][Assert] reason=package_path_empty"))
	{
		return nullptr;
	}

	const string packageCacheKey = buildPackageCacheKey(packageRelativePath);
	const auto foundPackage = packageCache.find(packageCacheKey);
	if (foundPackage != packageCache.end())
	{
		return foundPackage->second;
	}

	shared_pointer<ShaderPackageAsset> packageAsset(new ShaderPackageAsset());
	packageAsset->packageRelativePath = packageRelativePath;
	packageAsset->state = ShaderPackageState::pending;

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	string packageAbsolutePath = {};
	assert(diskLoaderModule != nullptr);
	const bool resolvedPackageAbsolutePath = diskLoaderModule->resolvePathFromResources(packageRelativePath, packageAbsolutePath);
	if (!resolvedPackageAbsolutePath)
	{
		packageAsset->state = ShaderPackageState::failed;
		packageCache.emplace(packageCacheKey, packageAsset);
		assert(false && "[ShaderPackageModule][Assert] reason=package_path_resolve_failed");
	}

	vector<ShaderPackageShaderRecord> shaderRecords = {};
	vector<ShaderPackageVariantRecord> variantRecords = {};
	if (!parseShaderPackageManifest(packageAbsolutePath, shaderRecords, variantRecords))
	{
		packageAsset->state = ShaderPackageState::failed;
		packageCache.emplace(packageCacheKey, packageAsset);
		return packageAsset;
	}

	unordered_map<string, shared_pointer<ShaderHandle>> shaderById;
	shaderById.reserve(shaderRecords.size());
	if (!resolveShaderRecords(packageRelativePath, shaderRecords, shaderById))
	{
		packageAsset->state = ShaderPackageState::failed;
		packageCache.emplace(packageCacheKey, packageAsset);
		return packageAsset;
	}

	if (!buildVariants(packageRelativePath, variantRecords, shaderById, packageAsset->variants))
	{
		packageAsset->state = ShaderPackageState::failed;
		packageCache.emplace(packageCacheKey, packageAsset);
		return packageAsset;
	}

	packageAsset->state = ShaderPackageState::ready;
	packageCache.emplace(packageCacheKey, packageAsset);
	output << "[ShaderPackageModule][Ready] package=" << packageRelativePath
		   << " variantCount=" << packageAsset->variants.size() << lineBreak;
	return packageAsset;
}

void ShaderPackageModule::clear()
{
	packageCache.clear();
}

uint32 ShaderPackageModule::getCachedPackageCount() const
{
	return static_cast<uint32>(packageCache.size());
}

string ShaderPackageModule::buildPackageCacheKey(const string& packageRelativePath) const
{
	return packageRelativePath;
}
