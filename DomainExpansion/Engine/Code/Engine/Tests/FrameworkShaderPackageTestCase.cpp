#include "Engine/Tests/FrameworkShaderPackageTestCase.h"

#include "Engine/Framework/Framework.h"
#include "Engine/Module/Asset/ShaderModule.h"
#include "Engine/Module/Asset/ShaderPackageModule.h"

static const ShaderPackageVariant* Temp_findShaderPackageVariantByName(
	const ShaderPackageAsset& shaderPackageAsset,
	const string& variantName)
{
	for (uint32 variantIndex = 0; variantIndex < static_cast<uint32>(shaderPackageAsset.variants.size()); ++variantIndex)
	{
		const ShaderPackageVariant& variant = shaderPackageAsset.variants[variantIndex];
		if (variant.name == variantName)
		{
			return &variant;
		}
	}

	return nullptr;
}

const char* FrameworkShaderPackageTestCase::getTestCaseName() const
{
	return "FrameworkShaderPackageTestCase";
}

bool FrameworkShaderPackageTestCase::beginTest(Framework& framework)
{
	unused(framework);

	bool beginResult = true;
	shared_pointer<ShaderModule> shaderModule = ShaderModule::get();
	shared_pointer<ShaderPackageModule> shaderPackageModule = ShaderPackageModule::get();
	beginResult = expectCondition(shaderModule != nullptr, "begin: shader module exists") && beginResult;
	beginResult = expectCondition(shaderPackageModule != nullptr, "begin: shader package module exists") && beginResult;
	if (!beginResult || shaderModule == nullptr || shaderPackageModule == nullptr)
	{
		return false;
	}

	shaderPackageModule->clear();
	shaderModule->clear();
	return expectCondition(
		shaderModule->getCachedShaderCount() == 0 && shaderPackageModule->getCachedPackageCount() == 0,
		"begin: clear shader caches");
}

bool FrameworkShaderPackageTestCase::runTest(Framework& framework)
{
	unused(framework);

	bool runResult = true;
	shared_pointer<ShaderModule> shaderModule = ShaderModule::get();
	shared_pointer<ShaderPackageModule> shaderPackageModule = ShaderPackageModule::get();
	runResult = expectCondition(shaderModule != nullptr, "run: shader module exists") && runResult;
	runResult = expectCondition(shaderPackageModule != nullptr, "run: shader package module exists") && runResult;
	if (!runResult || shaderModule == nullptr || shaderPackageModule == nullptr)
	{
		return false;
	}

	const string packageRelativePath = "Shaders/Packages/TestBasic.shaderpkg";
	shared_pointer<ShaderPackageAsset> shaderPackage = shaderPackageModule->getOrLoadPackage(packageRelativePath);
	runResult = expectCondition(shaderPackage != nullptr, "run: shader package load handle exists") && runResult;
	runResult = expectCondition(
		shaderPackage != nullptr && shaderPackage->state == ShaderPackageState::ready,
		"run: shader package load success") && runResult;
	runResult = expectCondition(
		shaderPackage != nullptr && shaderPackage->variants.size() == 2,
		"run: shader package variant count is 2") && runResult;

	const ShaderPackageVariant* graphicsVariant = shaderPackage != nullptr
		? Temp_findShaderPackageVariantByName(*shaderPackage, "GraphicsDefault")
		: nullptr;
	const ShaderPackageVariant* computeVariant = shaderPackage != nullptr
		? Temp_findShaderPackageVariantByName(*shaderPackage, "ComputeOnly")
		: nullptr;
	runResult = expectCondition(graphicsVariant != nullptr, "run: graphics variant exists") && runResult;
	runResult = expectCondition(computeVariant != nullptr, "run: compute variant exists") && runResult;

	shared_pointer<ShaderObject> graphicsVertexShader = graphicsVariant != nullptr
		? graphicsVariant->getShader(ShaderStage::vertex)
		: nullptr;
	shared_pointer<ShaderObject> graphicsPixelShader = graphicsVariant != nullptr
		? graphicsVariant->getShader(ShaderStage::pixel)
		: nullptr;
	shared_pointer<ShaderObject> computeShader = computeVariant != nullptr
		? computeVariant->getShader(ShaderStage::compute)
		: nullptr;

	runResult = expectCondition(
		graphicsVertexShader != nullptr && graphicsVertexShader->getShaderDataHash() != 0,
		"run: graphics vertex shader linked") && runResult;
	runResult = expectCondition(
		graphicsPixelShader != nullptr && graphicsPixelShader->getShaderDataHash() != 0,
		"run: graphics pixel shader linked") && runResult;
	runResult = expectCondition(
		computeShader != nullptr && computeShader->getShaderDataHash() != 0,
		"run: compute shader linked") && runResult;
	runResult = expectCondition(
		shaderModule->getCachedShaderCount() == 3 && shaderPackageModule->getCachedPackageCount() == 1,
		"run: initial cache counts") && runResult;

	const string geometryPackageRelativePath = "Shaders/Packages/GeometryBaseColor.shaderpkg";
	shared_pointer<ShaderPackageAsset> geometryShaderPackage = shaderPackageModule->getOrLoadPackage(geometryPackageRelativePath);
	runResult = expectCondition(geometryShaderPackage != nullptr, "run: geometry shader package load handle exists") && runResult;
	runResult = expectCondition(
		geometryShaderPackage != nullptr && geometryShaderPackage->state == ShaderPackageState::ready,
		"run: geometry shader package load success") && runResult;
	const ShaderPackageVariant* geometryVariant = geometryShaderPackage != nullptr
		? Temp_findShaderPackageVariantByName(*geometryShaderPackage, "GeometryDefault")
		: nullptr;
	runResult = expectCondition(geometryVariant != nullptr, "run: geometry variant exists") && runResult;
	runResult = expectCondition(
		geometryVariant != nullptr
		&& geometryVariant->getShader(ShaderStage::vertex) != nullptr
		&& geometryVariant->getShader(ShaderStage::pixel) != nullptr,
		"run: geometry shaders linked") && runResult;

	const string editorGridPackageRelativePath = "Shaders/Packages/EditorGrid.shaderpkg";
	shared_pointer<ShaderPackageAsset> editorGridShaderPackage = shaderPackageModule->getOrLoadPackage(editorGridPackageRelativePath);
	runResult = expectCondition(editorGridShaderPackage != nullptr, "run: editor grid shader package load handle exists") && runResult;
	runResult = expectCondition(
		editorGridShaderPackage != nullptr && editorGridShaderPackage->state == ShaderPackageState::ready,
		"run: editor grid shader package load success") && runResult;
	const ShaderPackageVariant* editorGridVariant = editorGridShaderPackage != nullptr
		? Temp_findShaderPackageVariantByName(*editorGridShaderPackage, "EditorGridDefault")
		: nullptr;
	runResult = expectCondition(editorGridVariant != nullptr, "run: editor grid variant exists") && runResult;
	runResult = expectCondition(
		editorGridVariant != nullptr
		&& editorGridVariant->getShader(ShaderStage::vertex) != nullptr
		&& editorGridVariant->getShader(ShaderStage::pixel) != nullptr,
		"run: editor grid shaders linked") && runResult;

	shared_pointer<ShaderPackageAsset> cachedShaderPackage = shaderPackageModule->getOrLoadPackage(packageRelativePath);
	runResult = expectCondition(
		cachedShaderPackage == shaderPackage,
		"run: package cache hit reuses pointer") && runResult;

	shaderPackageModule->clear();
	shared_pointer<ShaderPackageAsset> reloadedPackageAfterPackageClear = shaderPackageModule->getOrLoadPackage(packageRelativePath);
	runResult = expectCondition(
		reloadedPackageAfterPackageClear != nullptr
		&& reloadedPackageAfterPackageClear->state == ShaderPackageState::ready
		&& reloadedPackageAfterPackageClear != shaderPackage,
		"run: package clear forces package reload") && runResult;

	const ShaderPackageVariant* reloadedGraphicsVariantAfterPackageClear =
		reloadedPackageAfterPackageClear != nullptr
		? Temp_findShaderPackageVariantByName(*reloadedPackageAfterPackageClear, "GraphicsDefault")
		: nullptr;
	shared_pointer<ShaderObject> reloadedGraphicsVertexShaderAfterPackageClear =
		reloadedGraphicsVariantAfterPackageClear != nullptr
		? reloadedGraphicsVariantAfterPackageClear->getShader(ShaderStage::vertex)
		: nullptr;
	runResult = expectCondition(
		reloadedGraphicsVertexShaderAfterPackageClear == graphicsVertexShader,
		"run: package reload reuses shader cache") && runResult;

	shaderModule->clear();
	shaderPackageModule->clear();
	shared_pointer<ShaderPackageAsset> reloadedPackageAfterShaderClear = shaderPackageModule->getOrLoadPackage(packageRelativePath);
	const ShaderPackageVariant* reloadedGraphicsVariantAfterShaderClear =
		reloadedPackageAfterShaderClear != nullptr
		? Temp_findShaderPackageVariantByName(*reloadedPackageAfterShaderClear, "GraphicsDefault")
		: nullptr;
	shared_pointer<ShaderObject> reloadedGraphicsVertexShaderAfterShaderClear =
		reloadedGraphicsVariantAfterShaderClear != nullptr
		? reloadedGraphicsVariantAfterShaderClear->getShader(ShaderStage::vertex)
		: nullptr;
	runResult = expectCondition(
		reloadedPackageAfterShaderClear != nullptr
		&& reloadedPackageAfterShaderClear->state == ShaderPackageState::ready,
		"run: shader clear reloads package") && runResult;
	runResult = expectCondition(
		reloadedGraphicsVertexShaderAfterShaderClear != nullptr
		&& reloadedGraphicsVertexShaderAfterShaderClear != graphicsVertexShader,
		"run: shader clear forces shader reload") && runResult;

	return runResult;
}

bool FrameworkShaderPackageTestCase::endTest(Framework& framework)
{
	unused(framework);

	shared_pointer<ShaderModule> shaderModule = ShaderModule::get();
	shared_pointer<ShaderPackageModule> shaderPackageModule = ShaderPackageModule::get();
	if (shaderPackageModule != nullptr)
	{
		shaderPackageModule->clear();
	}
	if (shaderModule != nullptr)
	{
		shaderModule->clear();
	}

	return expectCondition(true, "end: shader package test cleanup");
}
