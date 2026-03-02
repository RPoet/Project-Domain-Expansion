#include "Engine/Tests/FrameworkRootSignatureLifecycleTestCase.h"

#include "Engine/Framework/Framework.h"
#include "Engine/Window/WindowsWindowObject.h"

const char* FrameworkRootSignatureLifecycleTestCase::getTestCaseName() const
{
	return "FrameworkRootSignatureLifecycleTestCase";
}

bool FrameworkRootSignatureLifecycleTestCase::beginTest(Framework& framework)
{
	testRenderBackend.reset();

	WindowsWindowObject* windowObject = framework.getWindowObject();
	bool beginResult = true;
	beginResult = expectCondition(
		windowObject != nullptr,
		"begin: window object exists") && beginResult;

	RenderBackendCreateOptions createOptions = {};
	if (windowObject != nullptr)
	{
		createOptions.windowHandle = windowObject->getWindowHandle();
		createOptions.width = windowObject->getClientWidth();
		createOptions.height = windowObject->getClientHeight();
	}
	createOptions.backendType = RenderBackendType::dx12;
	createOptions.enableDebugLayer = true;

	beginResult = expectCondition(
		createOptions.windowHandle != nullptr,
		"begin: window handle exists") && beginResult;
	beginResult = expectCondition(
		RenderBackend::isSupportedBackend(createOptions.backendType),
		"begin: dx12 backend supported") && beginResult;

	if (!beginResult)
	{
		return false;
	}

	testRenderBackend = RenderBackend::createBackend(createOptions.backendType);
	beginResult = expectCondition(
		testRenderBackend != nullptr,
		"begin: create backend object") && beginResult;
	if (!beginResult || testRenderBackend == nullptr)
	{
		return false;
	}

	beginResult = expectCondition(
		testRenderBackend->create(createOptions),
		"begin: create backend device") && beginResult;
	if (!beginResult)
	{
		testRenderBackend.reset();
		return false;
	}

	return true;
}

bool FrameworkRootSignatureLifecycleTestCase::runTest(Framework& framework)
{
	unused(framework);

	bool runResult = true;
	runResult = expectCondition(
		testRenderBackend != nullptr,
		"run: backend is ready") && runResult;
	if (!runResult || testRenderBackend == nullptr)
	{
		return false;
	}

	RootSignatureDesc rootSignatureDescA = {};
	PushConstantRange pushConstantRangeA = {};
	pushConstantRangeA.offsetInBytes = 0;
	pushConstantRangeA.sizeInBytes = 16;
	pushConstantRangeA.shaderVisibility = ShaderVisibility::allGraphics;
	rootSignatureDescA.pushConstantRanges.push_back(pushConstantRangeA);

	RootSignatureDesc rootSignatureDescB = {};
	PushConstantRange pushConstantRangeB = {};
	pushConstantRangeB.offsetInBytes = 0;
	pushConstantRangeB.sizeInBytes = 32;
	pushConstantRangeB.shaderVisibility = ShaderVisibility::allGraphics;
	rootSignatureDescB.pushConstantRanges.push_back(pushConstantRangeB);

	RootSignatureObject* rootSignatureA0 = testRenderBackend->getOrCreateRootSignatureObject(rootSignatureDescA);
	runResult = expectCondition(
		rootSignatureA0 != nullptr,
		"run: create root signature A") && runResult;

	RootSignatureObject* rootSignatureA1 = testRenderBackend->getOrCreateRootSignatureObject(rootSignatureDescA);
	runResult = expectCondition(
		rootSignatureA1 == rootSignatureA0 && rootSignatureA1 != nullptr,
		"run: cache hit for root signature A") && runResult;

	RootSignatureObject* rootSignatureB0 = testRenderBackend->getOrCreateRootSignatureObject(rootSignatureDescB);
	runResult = expectCondition(
		rootSignatureB0 != nullptr && rootSignatureB0 != rootSignatureA0,
		"run: create distinct root signature B") && runResult;

	testRenderBackend->clearRootSignatureObjects();

	RootSignatureObject* rootSignatureA2 = testRenderBackend->getOrCreateRootSignatureObject(rootSignatureDescA);
	runResult = expectCondition(
		rootSignatureA2 != nullptr,
		"run: recreate root signature A after clear") && runResult;

	RootSignatureObject* rootSignatureA3 = testRenderBackend->getOrCreateRootSignatureObject(rootSignatureDescA);
	runResult = expectCondition(
		rootSignatureA3 == rootSignatureA2 && rootSignatureA3 != nullptr,
		"run: cache hit for root signature A after clear") && runResult;

	return runResult;
}

bool FrameworkRootSignatureLifecycleTestCase::endTest(Framework& framework)
{
	unused(framework);

	if (testRenderBackend != nullptr)
	{
		testRenderBackend->destroy();
		testRenderBackend.reset();
	}

	return expectCondition(
		testRenderBackend == nullptr,
		"end: destroy backend object");
}
