#include "Engine/Tests/FrameworkPipelineStateLifecycleTestCase.h"

#include "Engine/Framework/Framework.h"
#include "Engine/Window/WindowsWindowObject.h"

#include <d3dcompiler.h>
#include <cstring>

static bool compileShaderByteCode(
	const char* shaderSource,
	const char* entryPoint,
	const char* profile,
	vector<char>& outByteCode)
{
	if (shaderSource == nullptr || entryPoint == nullptr || profile == nullptr)
	{
		return false;
	}

	using D3DCompileFunctionType = HRESULT(WINAPI*)(
		LPCVOID,
		SIZE_T,
		LPCSTR,
		const D3D_SHADER_MACRO*,
		ID3DInclude*,
		LPCSTR,
		LPCSTR,
		UINT,
		UINT,
		ID3DBlob**,
		ID3DBlob**);

	HMODULE compilerModule = LoadLibraryW(L"d3dcompiler_47.dll");
	if (compilerModule == nullptr)
	{
		error << "[FrameworkPipelineStateLifecycleTestCase][Error] reason=d3dcompiler_47_missing" << lineBreak;
		return false;
	}

	D3DCompileFunctionType d3dCompileFunction =
		reinterpret_cast<D3DCompileFunctionType>(GetProcAddress(compilerModule, "D3DCompile"));
	if (d3dCompileFunction == nullptr)
	{
		error << "[FrameworkPipelineStateLifecycleTestCase][Error] reason=d3dcompile_symbol_missing" << lineBreak;
		FreeLibrary(compilerModule);
		return false;
	}

	com_pointer<ID3DBlob> shaderBlob;
	com_pointer<ID3DBlob> errorBlob;
	const HRESULT compileResult = d3dCompileFunction(
		shaderSource,
		strlen(shaderSource),
		"FrameworkPipelineStateLifecycleTestCase",
		nullptr,
		nullptr,
		entryPoint,
		profile,
		0,
		0,
		&shaderBlob,
		&errorBlob);
	if (FAILED(compileResult) || shaderBlob == nullptr)
	{
		const char* compileErrorText = "compile_failed";
		if (errorBlob != nullptr && errorBlob->GetBufferPointer() != nullptr)
		{
			compileErrorText = static_cast<const char*>(errorBlob->GetBufferPointer());
		}

		error << "[FrameworkPipelineStateLifecycleTestCase][Error] reason=shader_compile_failed"
			  << " entry=" << entryPoint
			  << " profile=" << profile
			  << " text=" << compileErrorText << lineBreak;
		FreeLibrary(compilerModule);
		return false;
	}

	outByteCode.resize(static_cast<uint32>(shaderBlob->GetBufferSize()));
	if (!outByteCode.empty())
	{
		memcpy(outByteCode.data(), shaderBlob->GetBufferPointer(), outByteCode.size());
	}

	FreeLibrary(compilerModule);
	return true;
}

const char* FrameworkPipelineStateLifecycleTestCase::getTestCaseName() const
{
	return "FrameworkPipelineStateLifecycleTestCase";
}

bool FrameworkPipelineStateLifecycleTestCase::beginTest(Framework& framework)
{
	testRenderBackend.reset();

	bool beginResult = true;
	WindowsWindowObject* windowObject = framework.getWindowObject();
	beginResult = expectCondition(windowObject != nullptr, "begin: window object exists") && beginResult;

	RenderBackendCreateOptions createOptions = {};
	if (windowObject != nullptr)
	{
		createOptions.windowHandle = windowObject->getWindowHandle();
		createOptions.width = windowObject->getClientWidth();
		createOptions.height = windowObject->getClientHeight();
	}
	createOptions.backendType = RenderBackendType::dx12;
	createOptions.enableDebugLayer = true;

	beginResult = expectCondition(createOptions.windowHandle != nullptr, "begin: window handle exists") && beginResult;
	beginResult = expectCondition(
		RenderBackend::isSupportedBackend(createOptions.backendType),
		"begin: dx12 backend supported") && beginResult;
	if (!beginResult)
	{
		return false;
	}

	testRenderBackend = RenderBackend::createBackend(createOptions.backendType);
	beginResult = expectCondition(testRenderBackend != nullptr, "begin: create backend object") && beginResult;
	if (!beginResult || testRenderBackend == nullptr)
	{
		return false;
	}

	beginResult = expectCondition(testRenderBackend->create(createOptions), "begin: create backend device") && beginResult;
	if (!beginResult)
	{
		testRenderBackend.reset();
		return false;
	}

	return true;
}

bool FrameworkPipelineStateLifecycleTestCase::runTest(Framework& framework)
{
	unused(framework);

	bool runResult = true;
	runResult = expectCondition(testRenderBackend != nullptr, "run: backend is ready") && runResult;
	if (!runResult || testRenderBackend == nullptr)
	{
		return false;
	}

	const char* vertexShaderSource =
		"struct VSInput { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0; };"
		"struct VSOutput { float4 position : SV_Position; float2 uv : TEXCOORD0; };"
		"VSOutput mainVS(VSInput input) { VSOutput output; output.position = float4(input.position, 1.0f); output.uv = input.uv; return output; }";
	const char* pixelShaderSource =
		"struct PSInput { float4 position : SV_Position; float2 uv : TEXCOORD0; };"
		"float4 mainPS(PSInput input) : SV_Target0 { return float4(input.uv.x, input.uv.y, 0.0f, 1.0f); }";
	const char* computeShaderSource =
		"[numthreads(1, 1, 1)] void mainCS(uint3 dispatchThreadId : SV_DispatchThreadID) { }";

	shared_pointer<ShaderAsset> vertexShader(new ShaderAsset());
	shared_pointer<ShaderAsset> pixelShader(new ShaderAsset());
	shared_pointer<ShaderAsset> computeShader(new ShaderAsset());
	runResult = expectCondition(vertexShader != nullptr, "run: create vertex shader asset") && runResult;
	runResult = expectCondition(pixelShader != nullptr, "run: create pixel shader asset") && runResult;
	runResult = expectCondition(computeShader != nullptr, "run: create compute shader asset") && runResult;
	if (!runResult || vertexShader == nullptr || pixelShader == nullptr || computeShader == nullptr)
	{
		return false;
	}

	runResult = expectCondition(
		compileShaderByteCode(vertexShaderSource, "mainVS", "vs_5_0", vertexShader->byteCode),
		"run: compile vertex shader") && runResult;
	runResult = expectCondition(
		compileShaderByteCode(pixelShaderSource, "mainPS", "ps_5_0", pixelShader->byteCode),
		"run: compile pixel shader") && runResult;
	runResult = expectCondition(
		compileShaderByteCode(computeShaderSource, "mainCS", "cs_5_0", computeShader->byteCode),
		"run: compile compute shader") && runResult;
	if (!runResult)
	{
		return false;
	}

	RootSignatureDesc rootSignatureDesc = {};
	PushConstantRange pushConstantRange = {};
	pushConstantRange.offsetInBytes = 0;
	pushConstantRange.sizeInBytes = 16;
	pushConstantRange.shaderVisibility = ShaderVisibility::allGraphics;
	rootSignatureDesc.pushConstantRanges.push_back(pushConstantRange);

	PipelineStateDesc graphicsPipelineDescA = {};
	graphicsPipelineDescA.pipelineStateType = PipelineStateType::graphics;
	graphicsPipelineDescA.rootSignatureDesc = rootSignatureDesc;
	graphicsPipelineDescA.vertexShader = vertexShader;
	graphicsPipelineDescA.pixelShader = pixelShader;
	PipelineInputElementDesc positionInputElement = {};
	positionInputElement.semantic = VertexInputSemantic::position;
	positionInputElement.format = VertexInputFormat::float3;
	positionInputElement.inputSlot = 0;
	graphicsPipelineDescA.inputElements.push_back(positionInputElement);
	PipelineInputElementDesc normalInputElement = {};
	normalInputElement.semantic = VertexInputSemantic::normal;
	normalInputElement.format = VertexInputFormat::float3;
	normalInputElement.inputSlot = 1;
	graphicsPipelineDescA.inputElements.push_back(normalInputElement);
	PipelineInputElementDesc texcoordInputElement = {};
	texcoordInputElement.semantic = VertexInputSemantic::texcoord;
	texcoordInputElement.format = VertexInputFormat::float2;
	texcoordInputElement.inputSlot = 2;
	graphicsPipelineDescA.inputElements.push_back(texcoordInputElement);
	graphicsPipelineDescA.sampleCount = 1;
	PipelineRenderTargetDesc renderTargetDesc = {};
	renderTargetDesc.colorFormat = TextureFormat::rgba8Unorm;
	graphicsPipelineDescA.renderTargets.push_back(renderTargetDesc);
	graphicsPipelineDescA.depthStencilDesc.depthStencilFormat = TextureFormat::unknown;
	graphicsPipelineDescA.cullMode = PipelineCullMode::back;

	PipelineStateObject* graphicsPipelineA0 = testRenderBackend->getOrCreatePipelineStateObject(graphicsPipelineDescA);
	runResult = expectCondition(graphicsPipelineA0 != nullptr, "run: create graphics pipeline A") && runResult;
	PipelineStateObject* graphicsPipelineA1 = testRenderBackend->getOrCreatePipelineStateObject(graphicsPipelineDescA);
	runResult = expectCondition(
		graphicsPipelineA1 != nullptr && graphicsPipelineA1 == graphicsPipelineA0,
		"run: cache hit for graphics pipeline A") && runResult;

	PipelineStateDesc graphicsPipelineDescB = graphicsPipelineDescA;
	graphicsPipelineDescB.renderTargets[0].blendDesc.blendEnabled = true;
	graphicsPipelineDescB.renderTargets[0].blendDesc.sourceColorBlendFactor = PipelineBlendFactor::sourceAlpha;
	graphicsPipelineDescB.renderTargets[0].blendDesc.destinationColorBlendFactor = PipelineBlendFactor::inverseSourceAlpha;
	graphicsPipelineDescB.renderTargets[0].blendDesc.colorBlendOperation = PipelineBlendOperation::add;
	graphicsPipelineDescB.renderTargets[0].blendDesc.sourceAlphaBlendFactor = PipelineBlendFactor::one;
	graphicsPipelineDescB.renderTargets[0].blendDesc.destinationAlphaBlendFactor = PipelineBlendFactor::inverseSourceAlpha;
	graphicsPipelineDescB.renderTargets[0].blendDesc.alphaBlendOperation = PipelineBlendOperation::add;
	PipelineStateObject* graphicsPipelineB0 = testRenderBackend->getOrCreatePipelineStateObject(graphicsPipelineDescB);
	runResult = expectCondition(
		graphicsPipelineB0 != nullptr && graphicsPipelineB0 != graphicsPipelineA0,
		"run: create distinct graphics pipeline B") && runResult;

	PipelineStateDesc computePipelineDesc = {};
	computePipelineDesc.pipelineStateType = PipelineStateType::compute;
	computePipelineDesc.rootSignatureDesc = rootSignatureDesc;
	computePipelineDesc.computeShader = computeShader;
	computePipelineDesc.sampleCount = 1;
	PipelineStateObject* computePipeline0 = testRenderBackend->getOrCreatePipelineStateObject(computePipelineDesc);
	runResult = expectCondition(computePipeline0 != nullptr, "run: create compute pipeline") && runResult;
	PipelineStateObject* computePipeline1 = testRenderBackend->getOrCreatePipelineStateObject(computePipelineDesc);
	runResult = expectCondition(
		computePipeline1 != nullptr && computePipeline1 == computePipeline0,
		"run: cache hit for compute pipeline") && runResult;

	testRenderBackend->clearPipelineStateObjects();
	PipelineStateObject* graphicsPipelineA2 = testRenderBackend->getOrCreatePipelineStateObject(graphicsPipelineDescA);
	runResult = expectCondition(
		graphicsPipelineA2 != nullptr,
		"run: recreate graphics pipeline A after clear") && runResult;
	PipelineStateObject* graphicsPipelineA3 = testRenderBackend->getOrCreatePipelineStateObject(graphicsPipelineDescA);
	runResult = expectCondition(
		graphicsPipelineA3 != nullptr && graphicsPipelineA3 == graphicsPipelineA2,
		"run: cache hit for graphics pipeline A after clear") && runResult;

	return runResult;
}

bool FrameworkPipelineStateLifecycleTestCase::endTest(Framework& framework)
{
	unused(framework);

	if (testRenderBackend != nullptr)
	{
		testRenderBackend->destroy();
		testRenderBackend.reset();
	}

	return expectCondition(testRenderBackend == nullptr, "end: destroy backend object");
}
