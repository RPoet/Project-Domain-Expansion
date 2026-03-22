#include "Engine/Tests/FrameworkShaderAssetSmokeTestCase.h"

#include "Engine/Framework/Framework.h"
#include "Engine/Module/Asset/DiskLoaderModule.h"
#include "Render/Backends/Dx12/Dx12Shader.h"
#include "Render/Shader.h"

const char* FrameworkShaderAssetSmokeTestCase::getTestCaseName() const
{
	return "FrameworkShaderAssetSmokeTestCase";
}

bool FrameworkShaderAssetSmokeTestCase::beginTest(Framework& framework)
{
	unused(framework);
	return expectCondition(DiskLoaderModule::get() != nullptr, "begin: disk loader module exists");
}

bool FrameworkShaderAssetSmokeTestCase::runTest(Framework& framework)
{
	unused(framework);

	bool runResult = true;
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	runResult = expectCondition(diskLoaderModule != nullptr, "run: disk loader module exists") && runResult;
	if (!runResult || diskLoaderModule == nullptr)
	{
		return false;
	}

	string shaderAbsolutePath = {};
	runResult = expectCondition(
		diskLoaderModule->resolvePathFromResources("Shaders/Test/TestBasicVS.bin", shaderAbsolutePath),
		"run: resolve shader binary path") && runResult;
	if (!runResult)
	{
		return false;
	}

	vector<char> shaderByteCode = {};
	runResult = expectCondition(
		diskLoaderModule->loadBinaryFile(shaderAbsolutePath, shaderByteCode),
		"run: load shader binary through disk loader") && runResult;
	runResult = expectCondition(!shaderByteCode.empty(), "run: loaded shader binary is not empty") && runResult;
	if (!runResult)
	{
		return false;
	}

	shared_pointer<ShaderAsset> shaderAsset(new ShaderAsset());
	ShaderLoadRequest shaderLoadRequest = {};
	shaderLoadRequest.stage = ShaderStage::vertex;
	shaderLoadRequest.sourceRelativePath = "Shaders/Test/TestBasicVS.bin";
	shaderLoadRequest.entryPoint = "main";
	ShaderBinaryLoadRequest shaderBinaryLoadRequest = {};
	shaderBinaryLoadRequest.targetPlatform = ShaderTargetPlatform::dx12;
	shaderBinaryLoadRequest.binaryRelativePath = "Shaders/Test/TestBasicVS.bin";
	shaderBinaryLoadRequest.profile = "vs_6_6";
	runResult = expectCondition(shaderAsset != nullptr, "run: create shader asset") && runResult;
	runResult = expectCondition(
		shaderAsset != nullptr && shaderAsset->initialize(shaderLoadRequest),
		"run: initialize shader asset") && runResult;
	if (!runResult || shaderAsset == nullptr)
	{
		return false;
	}

	shared_pointer<Dx12ShaderObject> shaderObject(new Dx12ShaderObject());
	runResult = expectCondition(shaderObject != nullptr, "run: create dx12 shader") && runResult;
	runResult = expectCondition(
		shaderObject != nullptr && shaderObject->initialize(shaderAsset, shaderBinaryLoadRequest, moveValue(shaderByteCode)),
		"run: initialize dx12 shader") && runResult;
	runResult = expectCondition(
		shaderObject != nullptr && shaderObject->getAsset() != nullptr,
		"run: shader object owns logical asset") && runResult;
	runResult = expectCondition(
		shaderObject != nullptr && !shaderObject->getByteCode().empty(),
		"run: shader object exposes dx12 bytecode") && runResult;
	runResult = expectCondition(
		shaderObject != nullptr && shaderObject->getShaderDataHash() != 0,
		"run: shader object computes shader data hash") && runResult;

	return runResult;
}

bool FrameworkShaderAssetSmokeTestCase::endTest(Framework& framework)
{
	unused(framework);
	return expectCondition(true, "end: shader asset smoke cleanup");
}
