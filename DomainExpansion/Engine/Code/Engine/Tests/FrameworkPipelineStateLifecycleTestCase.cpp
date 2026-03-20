#include "Engine/Tests/FrameworkPipelineStateLifecycleTestCase.h"

#include "Engine/Framework/Framework.h"
#include "Engine/Window/WindowsWindowObject.h"
#include "Render/Backends/Dx12/Dx12Shader.h"

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
	const char* proceduralVertexShaderSource =
		"struct VSOutput { float4 position : SV_Position; };"
		"VSOutput mainVS(uint vertexId : SV_VertexID)"
		"{"
		"	float2 positions[6] = {"
		"		float2(-1.0f, -1.0f),"
		"		float2(-1.0f, 1.0f),"
		"		float2(1.0f, 1.0f),"
		"		float2(-1.0f, -1.0f),"
		"		float2(1.0f, 1.0f),"
		"		float2(1.0f, -1.0f)"
		"	};"
		"	VSOutput output;"
		"	output.position = float4(positions[vertexId], 0.0f, 1.0f);"
		"	return output;"
		"}";
	const char* solidPixelShaderSource =
		"float4 mainPS() : SV_Target0 { return float4(1.0f, 0.0f, 0.0f, 0.5f); }";

	vector<char> vertexShaderByteCode = {};
	vector<char> pixelShaderByteCode = {};
	vector<char> computeShaderByteCode = {};
	vector<char> proceduralVertexShaderByteCode = {};
	vector<char> solidPixelShaderByteCode = {};

	runResult = expectCondition(
		compileShaderByteCode(vertexShaderSource, "mainVS", "vs_5_0", vertexShaderByteCode),
		"run: compile vertex shader") && runResult;
	runResult = expectCondition(
		compileShaderByteCode(pixelShaderSource, "mainPS", "ps_5_0", pixelShaderByteCode),
		"run: compile pixel shader") && runResult;
	runResult = expectCondition(
		compileShaderByteCode(computeShaderSource, "mainCS", "cs_5_0", computeShaderByteCode),
		"run: compile compute shader") && runResult;
	runResult = expectCondition(
		compileShaderByteCode(proceduralVertexShaderSource, "mainVS", "vs_5_0", proceduralVertexShaderByteCode),
		"run: compile procedural vertex shader") && runResult;
	runResult = expectCondition(
		compileShaderByteCode(solidPixelShaderSource, "mainPS", "ps_5_0", solidPixelShaderByteCode),
		"run: compile solid pixel shader") && runResult;
	if (!runResult)
	{
		return false;
	}

	shared_pointer<ShaderAsset> vertexShaderAsset(new ShaderAsset());
	shared_pointer<ShaderAsset> pixelShaderAsset(new ShaderAsset());
	shared_pointer<ShaderAsset> computeShaderAsset(new ShaderAsset());
	shared_pointer<ShaderAsset> proceduralVertexShaderAsset(new ShaderAsset());
	shared_pointer<ShaderAsset> solidPixelShaderAsset(new ShaderAsset());
	runResult = expectCondition(vertexShaderAsset != nullptr, "run: create vertex shader asset") && runResult;
	runResult = expectCondition(pixelShaderAsset != nullptr, "run: create pixel shader asset") && runResult;
	runResult = expectCondition(computeShaderAsset != nullptr, "run: create compute shader asset") && runResult;
	runResult = expectCondition(proceduralVertexShaderAsset != nullptr, "run: create procedural vertex shader asset") && runResult;
	runResult = expectCondition(solidPixelShaderAsset != nullptr, "run: create solid pixel shader asset") && runResult;
	if (!runResult
		|| vertexShaderAsset == nullptr
		|| pixelShaderAsset == nullptr
		|| computeShaderAsset == nullptr
		|| proceduralVertexShaderAsset == nullptr
		|| solidPixelShaderAsset == nullptr)
	{
		return false;
	}

	ShaderLoadRequest vertexShaderLoadRequest = {};
	vertexShaderLoadRequest.stage = ShaderStage::vertex;
	vertexShaderLoadRequest.sourceRelativePath = "FrameworkPipelineStateLifecycleTestCase/Vertex.hlsl";
	vertexShaderLoadRequest.entryPoint = "mainVS";
	ShaderLoadRequest pixelShaderLoadRequest = {};
	pixelShaderLoadRequest.stage = ShaderStage::pixel;
	pixelShaderLoadRequest.sourceRelativePath = "FrameworkPipelineStateLifecycleTestCase/Pixel.hlsl";
	pixelShaderLoadRequest.entryPoint = "mainPS";
	ShaderLoadRequest computeShaderLoadRequest = {};
	computeShaderLoadRequest.stage = ShaderStage::compute;
	computeShaderLoadRequest.sourceRelativePath = "FrameworkPipelineStateLifecycleTestCase/Compute.hlsl";
	computeShaderLoadRequest.entryPoint = "mainCS";
	ShaderLoadRequest proceduralVertexShaderLoadRequest = {};
	proceduralVertexShaderLoadRequest.stage = ShaderStage::vertex;
	proceduralVertexShaderLoadRequest.sourceRelativePath = "FrameworkPipelineStateLifecycleTestCase/ProceduralVertex.hlsl";
	proceduralVertexShaderLoadRequest.entryPoint = "mainVS";
	ShaderLoadRequest solidPixelShaderLoadRequest = {};
	solidPixelShaderLoadRequest.stage = ShaderStage::pixel;
	solidPixelShaderLoadRequest.sourceRelativePath = "FrameworkPipelineStateLifecycleTestCase/SolidPixel.hlsl";
	solidPixelShaderLoadRequest.entryPoint = "mainPS";
	ShaderBinaryLoadRequest vertexShaderBinaryLoadRequest = {};
	vertexShaderBinaryLoadRequest.targetPlatform = ShaderTargetPlatform::dx12;
	vertexShaderBinaryLoadRequest.binaryRelativePath = "FrameworkPipelineStateLifecycleTestCase/Vertex.dxbc";
	vertexShaderBinaryLoadRequest.profile = "vs_5_0";
	ShaderBinaryLoadRequest pixelShaderBinaryLoadRequest = {};
	pixelShaderBinaryLoadRequest.targetPlatform = ShaderTargetPlatform::dx12;
	pixelShaderBinaryLoadRequest.binaryRelativePath = "FrameworkPipelineStateLifecycleTestCase/Pixel.dxbc";
	pixelShaderBinaryLoadRequest.profile = "ps_5_0";
	ShaderBinaryLoadRequest computeShaderBinaryLoadRequest = {};
	computeShaderBinaryLoadRequest.targetPlatform = ShaderTargetPlatform::dx12;
	computeShaderBinaryLoadRequest.binaryRelativePath = "FrameworkPipelineStateLifecycleTestCase/Compute.dxbc";
	computeShaderBinaryLoadRequest.profile = "cs_5_0";
	ShaderBinaryLoadRequest proceduralVertexShaderBinaryLoadRequest = {};
	proceduralVertexShaderBinaryLoadRequest.targetPlatform = ShaderTargetPlatform::dx12;
	proceduralVertexShaderBinaryLoadRequest.binaryRelativePath = "FrameworkPipelineStateLifecycleTestCase/ProceduralVertex.dxbc";
	proceduralVertexShaderBinaryLoadRequest.profile = "vs_5_0";
	ShaderBinaryLoadRequest solidPixelShaderBinaryLoadRequest = {};
	solidPixelShaderBinaryLoadRequest.targetPlatform = ShaderTargetPlatform::dx12;
	solidPixelShaderBinaryLoadRequest.binaryRelativePath = "FrameworkPipelineStateLifecycleTestCase/SolidPixel.dxbc";
	solidPixelShaderBinaryLoadRequest.profile = "ps_5_0";

	runResult = expectCondition(
		vertexShaderAsset->initialize(vertexShaderLoadRequest),
		"run: initialize vertex shader asset") && runResult;
	runResult = expectCondition(
		pixelShaderAsset->initialize(pixelShaderLoadRequest),
		"run: initialize pixel shader asset") && runResult;
	runResult = expectCondition(
		computeShaderAsset->initialize(computeShaderLoadRequest),
		"run: initialize compute shader asset") && runResult;
	runResult = expectCondition(
		proceduralVertexShaderAsset->initialize(proceduralVertexShaderLoadRequest),
		"run: initialize procedural vertex shader asset") && runResult;
	runResult = expectCondition(
		solidPixelShaderAsset->initialize(solidPixelShaderLoadRequest),
		"run: initialize solid pixel shader asset") && runResult;
	if (!runResult)
	{
		return false;
	}

	shared_pointer<Dx12ShaderObject> vertexShader(new Dx12ShaderObject());
	shared_pointer<Dx12ShaderObject> pixelShader(new Dx12ShaderObject());
	shared_pointer<Dx12ShaderObject> computeShader(new Dx12ShaderObject());
	shared_pointer<Dx12ShaderObject> proceduralVertexShader(new Dx12ShaderObject());
	shared_pointer<Dx12ShaderObject> solidPixelShader(new Dx12ShaderObject());
	runResult = expectCondition(vertexShader != nullptr, "run: create vertex shader object") && runResult;
	runResult = expectCondition(pixelShader != nullptr, "run: create pixel shader object") && runResult;
	runResult = expectCondition(computeShader != nullptr, "run: create compute shader object") && runResult;
	runResult = expectCondition(proceduralVertexShader != nullptr, "run: create procedural vertex shader object") && runResult;
	runResult = expectCondition(solidPixelShader != nullptr, "run: create solid pixel shader object") && runResult;
	runResult = expectCondition(
		vertexShader != nullptr && vertexShader->initialize(vertexShaderAsset, vertexShaderBinaryLoadRequest, moveValue(vertexShaderByteCode)),
		"run: initialize vertex shader object") && runResult;
	runResult = expectCondition(
		pixelShader != nullptr && pixelShader->initialize(pixelShaderAsset, pixelShaderBinaryLoadRequest, moveValue(pixelShaderByteCode)),
		"run: initialize pixel shader object") && runResult;
	runResult = expectCondition(
		computeShader != nullptr && computeShader->initialize(computeShaderAsset, computeShaderBinaryLoadRequest, moveValue(computeShaderByteCode)),
		"run: initialize compute shader object") && runResult;
	runResult = expectCondition(
		proceduralVertexShader != nullptr
			&& proceduralVertexShader->initialize(
				proceduralVertexShaderAsset,
				proceduralVertexShaderBinaryLoadRequest,
				moveValue(proceduralVertexShaderByteCode)),
		"run: initialize procedural vertex shader object") && runResult;
	runResult = expectCondition(
		solidPixelShader != nullptr
			&& solidPixelShader->initialize(
				solidPixelShaderAsset,
				solidPixelShaderBinaryLoadRequest,
				moveValue(solidPixelShaderByteCode)),
		"run: initialize solid pixel shader object") && runResult;
	if (!runResult
		|| vertexShader == nullptr
		|| pixelShader == nullptr
		|| computeShader == nullptr
		|| proceduralVertexShader == nullptr
		|| solidPixelShader == nullptr)
	{
		return false;
	}

	RootSignatureDesc rootSignatureDesc = {};
	PushConstantRange pushConstantRange = {};
	pushConstantRange.offsetInBytes = 0;
	pushConstantRange.sizeInBytes = 16;
	pushConstantRange.shaderVisibility = ShaderVisibility::allGraphics;
	rootSignatureDesc.pushConstantRanges.push_back(pushConstantRange);
	RootSignatureObject* pipelineRootSignatureObject = testRenderBackend->getOrCreateRootSignatureObject(rootSignatureDesc);
	runResult = expectCondition(
		pipelineRootSignatureObject != nullptr,
		"run: create root signature for pipeline bind") && runResult;
	if (!runResult)
	{
		return false;
	}

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
	graphicsPipelineDescA.depthStencilDesc.depthStencilFormat = TextureFormat::d32Float;
	graphicsPipelineDescA.depthStencilDesc.depthTestEnabled = true;
	graphicsPipelineDescA.depthStencilDesc.depthWriteEnabled = true;
	graphicsPipelineDescA.depthStencilDesc.depthCompareOperation = PipelineCompareOperation::lessEqual;
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

	PipelineStateDesc graphicsPipelineDescC = {};
	graphicsPipelineDescC.pipelineStateType = PipelineStateType::graphics;
	graphicsPipelineDescC.rootSignatureDesc = rootSignatureDesc;
	graphicsPipelineDescC.vertexShader = proceduralVertexShader;
	graphicsPipelineDescC.pixelShader = solidPixelShader;
	graphicsPipelineDescC.sampleCount = 1;
	graphicsPipelineDescC.renderTargets.push_back(renderTargetDesc);
	graphicsPipelineDescC.depthStencilDesc.depthStencilFormat = TextureFormat::d32Float;
	graphicsPipelineDescC.depthStencilDesc.depthTestEnabled = true;
	graphicsPipelineDescC.depthStencilDesc.depthWriteEnabled = false;
	graphicsPipelineDescC.depthStencilDesc.depthCompareOperation = PipelineCompareOperation::lessEqual;
	graphicsPipelineDescC.cullMode = PipelineCullMode::none;
	PipelineStateObject* graphicsPipelineC0 = testRenderBackend->getOrCreatePipelineStateObject(graphicsPipelineDescC);
	runResult = expectCondition(
		graphicsPipelineC0 != nullptr,
		"run: create graphics pipeline C without input layout") && runResult;
	PipelineStateObject* graphicsPipelineC1 = testRenderBackend->getOrCreatePipelineStateObject(graphicsPipelineDescC);
	runResult = expectCondition(
		graphicsPipelineC1 != nullptr && graphicsPipelineC1 == graphicsPipelineC0,
		"run: cache hit for graphics pipeline C without input layout") && runResult;

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

	CommandList* pipelineBindCommandList = testRenderBackend->acquireCommandList(CommandListType::graphics);
	runResult = expectCondition(pipelineBindCommandList != nullptr, "run: acquire command list for pipeline bind") && runResult;
	bool pipelineBindExecuted = false;
	RenderTargetView* pipelineBindRenderTargetView = nullptr;
	DepthStencilView* pipelineBindDepthStencilView = nullptr;
	unique_pointer<TextureResourceObject> pipelineBindDepthTextureObject = nullptr;
	if (pipelineBindCommandList != nullptr)
	{
		SwapChain* swapChain = testRenderBackend->getSwapChain();
		TextureResourceObject* backBufferResource = swapChain != nullptr ? swapChain->getCurrentBackBufferResource() : nullptr;
		if (backBufferResource != nullptr)
		{
			pipelineBindRenderTargetView = testRenderBackend->createRenderTargetView(backBufferResource);
		}

		TextureObjectCreateOptions depthTextureCreateOptions = {};
		depthTextureCreateOptions.width = 64;
		depthTextureCreateOptions.height = 64;
		depthTextureCreateOptions.format = TextureFormat::d32Float;
		depthTextureCreateOptions.flags = getTextureObjectFlag(TextureObjectFlag::allowDepthStencil);
		depthTextureCreateOptions.initialState = ResourceState::depthWrite;
		pipelineBindDepthTextureObject = testRenderBackend->createTextureObject(depthTextureCreateOptions);
		if (pipelineBindDepthTextureObject != nullptr)
		{
			pipelineBindDepthStencilView = testRenderBackend->createDepthStencilView(pipelineBindDepthTextureObject.get());
		}

		pipelineBindCommandList->reset();
		if (pipelineBindRenderTargetView != nullptr)
		{
			RenderTargetView* renderTargetViews[1] = { pipelineBindRenderTargetView };
			pipelineBindCommandList->setRenderTargets(renderTargetViews, 1, pipelineBindDepthStencilView);
			pipelineBindCommandList->clearRenderTarget(pipelineBindRenderTargetView, 0.0f, 0.0f, 0.0f, 1.0f);
		}
		if (pipelineBindDepthStencilView != nullptr)
		{
			pipelineBindCommandList->clearDepthStencil(pipelineBindDepthStencilView, 1.0f, 0);
		}

		pipelineBindCommandList->setPipeline(graphicsPipelineA0, pipelineRootSignatureObject);
		const uint32 pushConstantData[4] = { 1, 2, 3, 4 };
		pipelineBindCommandList->setGraphicsPushConstants(0, pushConstantData, static_cast<uint32>(sizeof(pushConstantData)));
		pipelineBindCommandList->setPipeline(graphicsPipelineC0, pipelineRootSignatureObject);
		pipelineBindCommandList->setPrimitiveTopology(PrimitiveTopology::triangleList);
		pipelineBindCommandList->draw(6, 1, 0, 0);
		pipelineBindCommandList->setPipeline(computePipeline0, pipelineRootSignatureObject);
		pipelineBindCommandList->close();
		testRenderBackend->releaseCommandList(pipelineBindCommandList);
		if (pipelineBindRenderTargetView != nullptr)
		{
			testRenderBackend->destroyRenderTargetView(pipelineBindRenderTargetView);
		}
		if (pipelineBindDepthStencilView != nullptr)
		{
			testRenderBackend->destroyDepthStencilView(pipelineBindDepthStencilView);
		}
		pipelineBindExecuted = true;
	}
	runResult = expectCondition(pipelineBindExecuted, "run: bind graphics and compute pipelines on command list") && runResult;

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
