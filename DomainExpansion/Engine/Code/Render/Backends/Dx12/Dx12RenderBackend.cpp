#include "Render/Backends/Dx12/Dx12RenderBackend.h"
#include "Render/Backends/Dx12/Dx12CommandList.h"
#include "Render/Backends/Dx12/Dx12CommandQueue.h"
#include "Render/Backends/Dx12/Dx12Converter.h"
#include "Render/Backends/Dx12/Dx12DepthStencilView.h"
#include "Render/Backends/Dx12/Dx12RenderTargetView.h"
#include "Render/Backends/Dx12/Dx12RootSignatureObject.h"
#include "Render/Backends/Dx12/Dx12ResourceObject.h"
#include "Render/Backends/Dx12/Dx12SwapChain.h"
#include "Render/Backends/Dx12/Dx12SyncObject.h"
#include <cstring>

static bool validateTextureObjectCreateOptions(
	ID3D12Device* device,
	const TextureObjectCreateOptions& createOptions)
{
	if (device == nullptr
		|| createOptions.width == 0
		|| createOptions.format == TextureFormat::unknown
		|| (createOptions.dimension != TextureDimension::texture1D && createOptions.height == 0)
		|| createOptions.depthOrArraySize == 0
		|| createOptions.depthOrArraySize > 0xFFFFu
		|| createOptions.mipLevels == 0
		|| createOptions.mipLevels > 0xFFFFu
		|| createOptions.sampleCount == 0)
	{
		return false;
	}

	return getDx12TextureDimension(createOptions.dimension) != D3D12_RESOURCE_DIMENSION_UNKNOWN
		&& getDx12TextureFormat(createOptions.format) != DXGI_FORMAT_UNKNOWN;
}

Dx12RenderBackend::Dx12RenderBackend()
{
	commandQueue.reset(new Dx12CommandQueue());
	swapChain.reset(new Dx12SwapChain());
	syncObject.reset(new Dx12SyncObject());

	graphicsCommandListPool.reserve(graphicsCommandListPoolCapacity);
	graphicsCommandListInUse.reserve(graphicsCommandListPoolCapacity);
	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListPoolCapacity; ++commandListIndex)
	{
		graphicsCommandListPool.push_back(unique_pointer<Dx12CommandList>(new Dx12CommandList()));
		graphicsCommandListInUse.push_back(false);
	}
}

CommandList* Dx12RenderBackend::acquireCommandList()
{
	return acquireCommandList(CommandListType::graphics);
}

CommandList* Dx12RenderBackend::acquireCommandList(const CommandListType commandListType)
{
	if (!supportsCommandListType(commandListType))
	{
		return nullptr;
	}

	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListPool.size(); ++commandListIndex)
	{
		if (graphicsCommandListInUse[commandListIndex])
		{
			continue;
		}

		graphicsCommandListInUse[commandListIndex] = true;
		return graphicsCommandListPool[commandListIndex].get();
	}

	CommandListInitializeOptions initializeOptions = {
		.renderBackend = this,
		.commandListType = commandListType,
	};
	unique_pointer<Dx12CommandList> dx12CommandList(new Dx12CommandList());
	if (!dx12CommandList->initialize(initializeOptions))
	{
		return nullptr;
	}

	graphicsCommandListPool.push_back(moveValue(dx12CommandList));
	graphicsCommandListInUse.push_back(true);
	return graphicsCommandListPool.back().get();
}

bool Dx12RenderBackend::supportsCommandListType(const CommandListType commandListType) const
{
	return commandListType == CommandListType::graphics;
}

void Dx12RenderBackend::releaseCommandList(CommandList* commandList)
{
	if (commandList == nullptr)
	{
		return;
	}

	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListPool.size(); ++commandListIndex)
	{
		if (graphicsCommandListPool[commandListIndex].get() != commandList)
		{
			continue;
		}

		graphicsCommandListInUse[commandListIndex] = false;
		return;
	}
}

void Dx12RenderBackend::queueCommandList(CommandList* commandList)
{
	if (commandList == nullptr || commandQueue == nullptr)
	{
		return;
	}

	commandQueue->enqueue(commandList);
	queuedCommandLists.push_back(commandList);
}

void Dx12RenderBackend::executeQueuedCommandLists()
{
	if (commandQueue == nullptr)
	{
		return;
	}

	commandQueue->executeQueued();

	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(commandQueue.get());
	if (frameTimestampFence == nullptr || pendingFrameTimestampFenceValue == 0 || dx12CommandQueue == nullptr || dx12CommandQueue->getNativeCommandQueue() == nullptr)
	{
		return;
	}

	if (FAILED(dx12CommandQueue->getNativeCommandQueue()->Signal(frameTimestampFence.Get(), pendingFrameTimestampFenceValue)))
	{
		pendingFrameTimestampFenceValue = 0;
		gpuFrameTimeMilliseconds = 0.0f;
		return;
	}

	nextFrameTimestampFenceValue = pendingFrameTimestampFenceValue + 1;
}

CommandQueue* Dx12RenderBackend::getCommandQueue()
{
	return commandQueue.get();
}

SwapChain* Dx12RenderBackend::getSwapChain()
{
	return swapChain.get();
}

unique_pointer<SyncObject> Dx12RenderBackend::createSyncObject()
{
	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(commandQueue.get());
	const bool validStandaloneSyncContext = device != nullptr && dx12CommandQueue != nullptr;
	assert(validStandaloneSyncContext && "[Dx12RenderBackend][Assert] reason=standalone_sync_context_invalid");

	unique_pointer<Dx12SyncObject> createdSyncObject(new Dx12SyncObject());
	const bool standaloneSyncInitialized = createdSyncObject->initialize(device, dx12CommandQueue);
	assert(standaloneSyncInitialized && "[Dx12RenderBackend][Assert] reason=standalone_sync_initialize_failed");
	return moveValue(createdSyncObject);
}

unique_pointer<BufferResourceObject> Dx12RenderBackend::createBufferObject(
	const BufferObjectCreateOptions& createOptions)
{
	if (device == nullptr || createOptions.sizeInBytes == 0)
	{
		return nullptr;
	}

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = getDx12BufferHeapType(createOptions.memoryType);
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDescription = {};
	resourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDescription.Alignment = 0;
	resourceDescription.Width = createOptions.sizeInBytes;
	resourceDescription.Height = 1;
	resourceDescription.DepthOrArraySize = 1;
	resourceDescription.MipLevels = 1;
	resourceDescription.Format = DXGI_FORMAT_UNKNOWN;
	resourceDescription.SampleDesc.Count = 1;
	resourceDescription.SampleDesc.Quality = 0;
	resourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

	com_pointer<ID3D12Resource> dx12BufferResource;
	if (FAILED(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDescription,
		getDx12BufferInitialState(createOptions.memoryType),
		nullptr,
		IID_PPV_ARGS(&dx12BufferResource))))
	{
		return nullptr;
	}

	unique_pointer<Dx12BufferObject> createdBufferObject(new Dx12BufferObject());
	createdBufferObject->getUnderlyingResource() = dx12BufferResource;
	return createdBufferObject;
}

unique_pointer<TextureResourceObject> Dx12RenderBackend::createTextureObject(
	const TextureObjectCreateOptions& createOptions)
{
	const bool validCreateOptions = validateTextureObjectCreateOptions(device.Get(), createOptions);
	assert(validCreateOptions);

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDescription = {};
	resourceDescription.Dimension = getDx12TextureDimension(createOptions.dimension);
	resourceDescription.Alignment = createOptions.alignment;
	resourceDescription.Width = createOptions.width;
	resourceDescription.Height = createOptions.height;
	resourceDescription.DepthOrArraySize = static_cast<uint16>(createOptions.depthOrArraySize);
	resourceDescription.MipLevels = static_cast<uint16>(createOptions.mipLevels);
	resourceDescription.Format = getDx12TextureFormat(createOptions.format);
	resourceDescription.SampleDesc.Count = createOptions.sampleCount;
	resourceDescription.SampleDesc.Quality = createOptions.sampleQuality;
	resourceDescription.Layout = getDx12TextureLayout(createOptions.layout);
	resourceDescription.Flags = getDx12TextureResourceFlags(createOptions.flags);

	D3D12_CLEAR_VALUE clearValue = {};
	D3D12_CLEAR_VALUE* clearValuePointer = nullptr;
	if ((createOptions.flags & getTextureObjectFlag(TextureObjectFlag::allowDepthStencil)) != 0)
	{
		clearValue.Format = resourceDescription.Format;
		clearValue.DepthStencil.Depth = createOptions.clearDepth;
		clearValue.DepthStencil.Stencil = static_cast<UINT8>(createOptions.clearStencil & 0xFFu);
		clearValuePointer = &clearValue;
	}
	else if ((createOptions.flags & getTextureObjectFlag(TextureObjectFlag::allowRenderTarget)) != 0)
	{
		clearValue.Format = resourceDescription.Format;
		clearValue.Color[0] = createOptions.clearColors[0];
		clearValue.Color[1] = createOptions.clearColors[1];
		clearValue.Color[2] = createOptions.clearColors[2];
		clearValue.Color[3] = createOptions.clearColors[3];
		clearValuePointer = &clearValue;
	}

	com_pointer<ID3D12Resource> dx12TextureResource;
	if (FAILED(device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDescription,
		getDx12ResourceState(createOptions.initialState),
		clearValuePointer,
		IID_PPV_ARGS(&dx12TextureResource))))
	{
		return nullptr;
	}

	unique_pointer<Dx12TextureResourceObject> createdTextureObject(new Dx12TextureResourceObject());
	createdTextureObject->getUnderlyingResource() = dx12TextureResource;
	return createdTextureObject;
}

RootSignatureObject* Dx12RenderBackend::getOrCreateRootSignatureObject(const RootSignatureDesc& rootSignatureDesc)
{
	Dx12RootSignatureDesc dx12RootSignatureDesc = {};
	dx12RootSignatureDesc.flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	for (uint32 rangeIndex = 0; rangeIndex < static_cast<uint32>(rootSignatureDesc.pushConstantRanges.size()); ++rangeIndex)
	{
		const PushConstantRange& pushConstantRange = rootSignatureDesc.pushConstantRanges[rangeIndex];
		const bool pushConstantSizeZero = pushConstantRange.sizeInBytes == 0;
		const bool pushConstantOffsetMisaligned = (pushConstantRange.offsetInBytes & 3u) != 0;
		const bool pushConstantSizeMisaligned = (pushConstantRange.sizeInBytes & 3u) != 0;
		const bool validPushConstantRange = !pushConstantSizeZero
			&& !pushConstantOffsetMisaligned
			&& !pushConstantSizeMisaligned;
		assert(validPushConstantRange && "[Dx12RootSignature][Assert] reason=push_constant_invalid");

		D3D12_ROOT_PARAMETER rootParameter = {};
		rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParameter.ShaderVisibility = getDx12ShaderVisibility(pushConstantRange.shaderVisibility);
		// TO DO : Replace temporary offset->register mapping when root-constant binder is finalized.
		rootParameter.Constants.ShaderRegister = pushConstantRange.offsetInBytes / 4u;
		rootParameter.Constants.RegisterSpace = 0;
		rootParameter.Constants.Num32BitValues = pushConstantRange.sizeInBytes / 4u;
		dx12RootSignatureDesc.rootParameters.push_back(rootParameter);
	}

	const uint64 rootSignatureHash = dx12RootSignatureDesc.getHashValue();
	RootSignatureObject* foundRootSignatureObject = rootSignatureManager.find(rootSignatureHash, dx12RootSignatureDesc);
	if (foundRootSignatureObject != nullptr)
	{
		return foundRootSignatureObject;
	}

	assert(device != nullptr);

	const D3D12_ROOT_SIGNATURE_DESC rootSignatureDescription = dx12RootSignatureDesc.getNativeDesc();

	com_pointer<ID3DBlob> serializedRootSignature;
	com_pointer<ID3DBlob> errorBlob;
	const bool serializedRootSignatureCreated = SUCCEEDED(D3D12SerializeRootSignature(
		&rootSignatureDescription,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&serializedRootSignature,
		&errorBlob));
	assert(serializedRootSignatureCreated && "[Dx12RootSignature][Assert] reason=serialize_root_signature_failed");

	com_pointer<ID3D12RootSignature> rootSignature;
	const bool rootSignatureCreated = SUCCEEDED(device->CreateRootSignature(
		0,
		serializedRootSignature->GetBufferPointer(),
		serializedRootSignature->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)));
	assert(rootSignatureCreated && "[Dx12RootSignature][Assert] reason=create_root_signature_failed");

	return rootSignatureManager.addOrGet(rootSignatureHash, dx12RootSignatureDesc, Dx12RootSignatureObject(dx12RootSignatureDesc, rootSignature));
}

PipelineStateObject* Dx12RenderBackend::getOrCreatePipelineStateObject(const PipelineStateDesc& pipelineStateDesc)
{
	Dx12PipelineStateDesc dx12PipelineStateDesc = {};
	dx12PipelineStateDesc.pipelineStateType = pipelineStateDesc.pipelineStateType;
	dx12PipelineStateDesc.inputElements = pipelineStateDesc.inputElements;
	dx12PipelineStateDesc.wireframe = pipelineStateDesc.wireframe;
	dx12PipelineStateDesc.sampleCount = pipelineStateDesc.sampleCount;
	dx12PipelineStateDesc.renderTargets = pipelineStateDesc.renderTargets;
	dx12PipelineStateDesc.depthStencilDesc = pipelineStateDesc.depthStencilDesc;
	dx12PipelineStateDesc.cullMode = pipelineStateDesc.cullMode;
	const bool validSampleCount = pipelineStateDesc.sampleCount != 0;
	assert(validSampleCount && "[Dx12PipelineState][Assert] reason=sample_count_zero");

	RootSignatureObject* rootSignatureObject = getOrCreateRootSignatureObject(pipelineStateDesc.rootSignatureDesc);
	const bool validRootSignatureObject = rootSignatureObject != nullptr;
	assert(validRootSignatureObject && "[Dx12PipelineState][Assert] reason=root_signature_create_failed");

	Dx12RootSignatureObject* dx12RootSignatureObject = static_cast<Dx12RootSignatureObject*>(rootSignatureObject);
	dx12PipelineStateDesc.rootSignatureHash = dx12RootSignatureObject->getPlatformRootSignatureDesc().getHashValue();

	if (pipelineStateDesc.pipelineStateType == PipelineStateType::graphics)
	{
		const bool missingVertexShader = pipelineStateDesc.vertexShader == nullptr;
		const bool missingPixelShader = pipelineStateDesc.pixelShader == nullptr;
		const bool missingRenderTargets = pipelineStateDesc.renderTargets.empty();
		const bool tooManyRenderTargets = pipelineStateDesc.renderTargets.size() > D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
		bool invalidRenderTargetFormat = false;
		for (uint32 renderTargetIndex = 0; renderTargetIndex < static_cast<uint32>(pipelineStateDesc.renderTargets.size()); ++renderTargetIndex)
		{
			if (pipelineStateDesc.renderTargets[renderTargetIndex].colorFormat == TextureFormat::unknown)
			{
				invalidRenderTargetFormat = true;
				break;
			}
		}

		const bool hasDepthStencilFormat = pipelineStateDesc.depthStencilDesc.depthStencilFormat != TextureFormat::unknown;
		const bool invalidDepthStencilState = !hasDepthStencilFormat
			&& (pipelineStateDesc.depthStencilDesc.depthTestEnabled
				|| pipelineStateDesc.depthStencilDesc.depthWriteEnabled
				|| pipelineStateDesc.depthStencilDesc.stencilEnabled);
		const bool validGraphicsPipeline = !missingVertexShader
			&& !missingPixelShader
			&& !missingRenderTargets
			&& !tooManyRenderTargets
			&& !invalidRenderTargetFormat
			&& !invalidDepthStencilState;
		assert(validGraphicsPipeline && "[Dx12PipelineState][Assert] reason=graphics_pipeline_invalid");

		const Dx12ShaderObject* dx12VertexShader = static_cast<const Dx12ShaderObject*>(pipelineStateDesc.vertexShader.get());
		const Dx12ShaderObject* dx12PixelShader = static_cast<const Dx12ShaderObject*>(pipelineStateDesc.pixelShader.get());

		dx12PipelineStateDesc.vertexShaderHash = dx12VertexShader->getShaderDataHash();
		dx12PipelineStateDesc.pixelShaderHash = dx12PixelShader->getShaderDataHash();
		dx12PipelineStateDesc.vertexShaderByteCodeSize = static_cast<uint32>(dx12VertexShader->getByteCode().size());
		dx12PipelineStateDesc.pixelShaderByteCodeSize = static_cast<uint32>(dx12PixelShader->getByteCode().size());
	}
	else if (pipelineStateDesc.pipelineStateType == PipelineStateType::compute)
	{
		const bool validComputePipeline = pipelineStateDesc.computeShader != nullptr;
		assert(validComputePipeline && "[Dx12PipelineState][Assert] reason=compute_shader_missing");

		const Dx12ShaderObject* dx12ComputeShader = static_cast<const Dx12ShaderObject*>(pipelineStateDesc.computeShader.get());

		dx12PipelineStateDesc.computeShaderHash = dx12ComputeShader->getShaderDataHash();
		dx12PipelineStateDesc.computeShaderByteCodeSize = static_cast<uint32>(dx12ComputeShader->getByteCode().size());
	}
	else
	{
		assert(false && "[Dx12PipelineState][Assert] reason=pipeline_type_invalid");
	}

	const uint64 pipelineStateHash = dx12PipelineStateDesc.getHashValue();
	PipelineStateObject* foundPipelineStateObject =
		pipelineStateManager.find(pipelineStateHash, dx12PipelineStateDesc);
	if (foundPipelineStateObject != nullptr)
	{
		return foundPipelineStateObject;
	}

	assert(device != nullptr);
	com_pointer<ID3D12PipelineState> pipelineState;
	if (pipelineStateDesc.pipelineStateType == PipelineStateType::graphics)
	{
		const Dx12ShaderObject* dx12VertexShader = static_cast<const Dx12ShaderObject*>(pipelineStateDesc.vertexShader.get());
		const Dx12ShaderObject* dx12PixelShader = static_cast<const Dx12ShaderObject*>(pipelineStateDesc.pixelShader.get());

		const vector<char>& vertexShaderByteCode = dx12VertexShader->getByteCode();
		const vector<char>& pixelShaderByteCode = dx12PixelShader->getByteCode();
		vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescriptions = {};
		inputElementDescriptions.reserve(pipelineStateDesc.inputElements.size());
		for (uint32 elementIndex = 0; elementIndex < static_cast<uint32>(pipelineStateDesc.inputElements.size()); ++elementIndex)
		{
			const PipelineInputElementDesc& inputElement = pipelineStateDesc.inputElements[elementIndex];
			const char* semanticName = getDx12VertexInputSemanticName(inputElement.semantic);
			const DXGI_FORMAT inputFormat = getDx12VertexInputFormat(inputElement.format);
			const bool validInputElement = semanticName != nullptr && inputFormat != DXGI_FORMAT_UNKNOWN;
			assert(validInputElement && "[Dx12PipelineState][Assert] reason=input_layout_element_invalid");

			D3D12_INPUT_ELEMENT_DESC dx12InputElement = {};
			dx12InputElement.SemanticName = semanticName;
			dx12InputElement.SemanticIndex = inputElement.semanticIndex;
			dx12InputElement.Format = inputFormat;
			dx12InputElement.InputSlot = inputElement.inputSlot;
			dx12InputElement.AlignedByteOffset = inputElement.alignedByteOffset;
			dx12InputElement.InputSlotClass = getDx12VertexInputClassification(inputElement.inputClassification);
			dx12InputElement.InstanceDataStepRate = inputElement.instanceDataStepRate;
			inputElementDescriptions.push_back(dx12InputElement);
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc = {};
		graphicsPipelineStateDesc.pRootSignature = dx12RootSignatureObject->getRootSignature().Get();
		graphicsPipelineStateDesc.VS = { vertexShaderByteCode.data(), vertexShaderByteCode.size() };
		graphicsPipelineStateDesc.PS = { pixelShaderByteCode.data(), pixelShaderByteCode.size() };
		graphicsPipelineStateDesc.InputLayout = {
			inputElementDescriptions.data(),
			static_cast<uint32>(inputElementDescriptions.size())
		};

		D3D12_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = pipelineStateDesc.wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = getDx12CullMode(pipelineStateDesc.cullMode);
		rasterizerDesc.FrontCounterClockwise = boolFalse;
		rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterizerDesc.DepthClipEnable = boolTrue;
		rasterizerDesc.MultisampleEnable = boolFalse;
		rasterizerDesc.AntialiasedLineEnable = boolFalse;
		rasterizerDesc.ForcedSampleCount = 0;
		rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

		D3D12_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = boolFalse;
		blendDesc.IndependentBlendEnable = pipelineStateDesc.renderTargets.size() > 1 ? boolTrue : boolFalse;
		const PipelineRenderTargetBlendDesc defaultRenderTargetBlendDesc = {};
		for (uint32 renderTargetIndex = 0; renderTargetIndex < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++renderTargetIndex)
		{
			const bool hasRenderTargetDesc = renderTargetIndex < static_cast<uint32>(pipelineStateDesc.renderTargets.size());
			const PipelineRenderTargetBlendDesc& selectedBlendDesc = hasRenderTargetDesc
				? pipelineStateDesc.renderTargets[renderTargetIndex].blendDesc
				: defaultRenderTargetBlendDesc;
			blendDesc.RenderTarget[renderTargetIndex] = getDx12RenderTargetBlendDesc(selectedBlendDesc);
		}

		graphicsPipelineStateDesc.BlendState = blendDesc;

		D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
		const bool hasDepthStencil = pipelineStateDesc.depthStencilDesc.depthStencilFormat != TextureFormat::unknown;
		depthStencilDesc.DepthEnable = hasDepthStencil && pipelineStateDesc.depthStencilDesc.depthTestEnabled;
		depthStencilDesc.DepthWriteMask = (hasDepthStencil && pipelineStateDesc.depthStencilDesc.depthWriteEnabled)
			? D3D12_DEPTH_WRITE_MASK_ALL
			: D3D12_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc = getDx12CompareOperation(pipelineStateDesc.depthStencilDesc.depthCompareOperation);
		depthStencilDesc.StencilEnable = hasDepthStencil && pipelineStateDesc.depthStencilDesc.stencilEnabled;
		depthStencilDesc.StencilReadMask = static_cast<UINT8>(pipelineStateDesc.depthStencilDesc.stencilReadMask & 0xFFu);
		depthStencilDesc.StencilWriteMask = static_cast<UINT8>(pipelineStateDesc.depthStencilDesc.stencilWriteMask & 0xFFu);
		depthStencilDesc.FrontFace.StencilFailOp = getDx12StencilOperation(pipelineStateDesc.depthStencilDesc.frontFace.stencilFailOperation);
		depthStencilDesc.FrontFace.StencilDepthFailOp = getDx12StencilOperation(pipelineStateDesc.depthStencilDesc.frontFace.stencilDepthFailOperation);
		depthStencilDesc.FrontFace.StencilPassOp = getDx12StencilOperation(pipelineStateDesc.depthStencilDesc.frontFace.stencilPassOperation);
		depthStencilDesc.FrontFace.StencilFunc = getDx12CompareOperation(pipelineStateDesc.depthStencilDesc.frontFace.stencilCompareOperation);
		depthStencilDesc.BackFace.StencilFailOp = getDx12StencilOperation(pipelineStateDesc.depthStencilDesc.backFace.stencilFailOperation);
		depthStencilDesc.BackFace.StencilDepthFailOp = getDx12StencilOperation(pipelineStateDesc.depthStencilDesc.backFace.stencilDepthFailOperation);
		depthStencilDesc.BackFace.StencilPassOp = getDx12StencilOperation(pipelineStateDesc.depthStencilDesc.backFace.stencilPassOperation);
		depthStencilDesc.BackFace.StencilFunc = getDx12CompareOperation(pipelineStateDesc.depthStencilDesc.backFace.stencilCompareOperation);
		graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;

		graphicsPipelineStateDesc.SampleMask = 0xFFFFFFFFu;
		graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		graphicsPipelineStateDesc.NumRenderTargets = static_cast<uint32>(pipelineStateDesc.renderTargets.size());
		for (uint32 renderTargetIndex = 0; renderTargetIndex < graphicsPipelineStateDesc.NumRenderTargets; ++renderTargetIndex)
		{
			graphicsPipelineStateDesc.RTVFormats[renderTargetIndex] = getDx12TextureFormat(pipelineStateDesc.renderTargets[renderTargetIndex].colorFormat);
		}
		graphicsPipelineStateDesc.DSVFormat = getDx12TextureFormat(pipelineStateDesc.depthStencilDesc.depthStencilFormat);
		graphicsPipelineStateDesc.SampleDesc.Count = pipelineStateDesc.sampleCount;
		graphicsPipelineStateDesc.SampleDesc.Quality = 0;
		graphicsPipelineStateDesc.NodeMask = 0;
		graphicsPipelineStateDesc.CachedPSO = {};
		graphicsPipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		const bool graphicsPipelineStateCreated = SUCCEEDED(device->CreateGraphicsPipelineState(
			&graphicsPipelineStateDesc,
			IID_PPV_ARGS(&pipelineState)));
		assert(graphicsPipelineStateCreated && "[Dx12PipelineState][Assert] reason=create_graphics_pso_failed");
	}
	else if (pipelineStateDesc.pipelineStateType == PipelineStateType::compute)
	{
		const Dx12ShaderObject* dx12ComputeShader = static_cast<const Dx12ShaderObject*>(pipelineStateDesc.computeShader.get());

		const vector<char>& computeShaderByteCode = dx12ComputeShader->getByteCode();
		D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc = {};
		computePipelineStateDesc.pRootSignature = dx12RootSignatureObject->getRootSignature().Get();
		computePipelineStateDesc.CS = { computeShaderByteCode.data(), computeShaderByteCode.size() };
		computePipelineStateDesc.NodeMask = 0;
		computePipelineStateDesc.CachedPSO = {};
		computePipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		const bool computePipelineStateCreated = SUCCEEDED(device->CreateComputePipelineState(
			&computePipelineStateDesc,
			IID_PPV_ARGS(&pipelineState)));
		assert(computePipelineStateCreated && "[Dx12PipelineState][Assert] reason=create_compute_pso_failed");
	}
	else
	{
		assert(false && "[Dx12PipelineState][Assert] reason=pipeline_type_unsupported");
	}

	Dx12PipelineStateObject createdPipelineStateObject(dx12PipelineStateDesc, pipelineState);
	return pipelineStateManager.addOrGet(pipelineStateHash, dx12PipelineStateDesc, moveValue(createdPipelineStateObject));
}

void Dx12RenderBackend::clearRootSignatureObjects()
{
	rootSignatureManager.clear();
}

void Dx12RenderBackend::clearPipelineStateObjects()
{
	pipelineStateManager.clear();
}

RenderTargetView* Dx12RenderBackend::createRenderTargetView(TextureResourceObject* textureResourceObject)
{
	// TO DO : Replace per-view descriptor heap allocation with descriptor/view allocator module.
	if (textureResourceObject == nullptr)
	{
		return nullptr;
	}

	Dx12TextureResourceObject* dx12TextureResourceObject = static_cast<Dx12TextureResourceObject*>(textureResourceObject);
	if (device == nullptr
		|| dx12TextureResourceObject->getUnderlyingResource() == nullptr)
	{
		return nullptr;
	}

	com_pointer<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDescription = {};
	descriptorHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptorHeapDescription.NumDescriptors = 1;
	descriptorHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descriptorHeapDescription.NodeMask = 0;
	if (FAILED(device->CreateDescriptorHeap(&descriptorHeapDescription, IID_PPV_ARGS(&descriptorHeap))))
	{
		return nullptr;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateRenderTargetView(dx12TextureResourceObject->getUnderlyingResource().Get(), nullptr, descriptorHandle);

	unique_pointer<Dx12RenderTargetView> renderTargetView(new Dx12RenderTargetView());
	renderTargetView->descriptorHeap = descriptorHeap;
	renderTargetView->descriptorHandle = descriptorHandle;
	return renderTargetView.release();
}

void Dx12RenderBackend::destroyRenderTargetView(RenderTargetView* renderTargetView)
{
	delete renderTargetView;
}

// TO DO : refactor view system.
DepthStencilView* Dx12RenderBackend::createDepthStencilView(TextureResourceObject* textureResourceObject)
{
	if (textureResourceObject == nullptr)
	{
		return nullptr;
	}

	Dx12TextureResourceObject* dx12TextureResourceObject = static_cast<Dx12TextureResourceObject*>(textureResourceObject);
	if (device == nullptr
		|| dx12TextureResourceObject->getUnderlyingResource() == nullptr)
	{
		return nullptr;
	}

	const D3D12_RESOURCE_DESC resourceDescription = dx12TextureResourceObject->getUnderlyingResource()->GetDesc();
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDescription = {};
	descriptorHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	descriptorHeapDescription.NumDescriptors = 1;
	descriptorHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descriptorHeapDescription.NodeMask = 0;

	com_pointer<ID3D12DescriptorHeap> descriptorHeap;
	if (FAILED(device->CreateDescriptorHeap(&descriptorHeapDescription, IID_PPV_ARGS(&descriptorHeap))))
	{
		return nullptr;
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDescription = {};
	depthStencilViewDescription.Format = resourceDescription.Format;
	depthStencilViewDescription.ViewDimension = resourceDescription.SampleDesc.Count > 1
		? D3D12_DSV_DIMENSION_TEXTURE2DMS
		: D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDescription.Flags = D3D12_DSV_FLAG_NONE;

	const D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateDepthStencilView(
		dx12TextureResourceObject->getUnderlyingResource().Get(),
		&depthStencilViewDescription,
		descriptorHandle);

	unique_pointer<Dx12DepthStencilView> depthStencilView(new Dx12DepthStencilView());
	depthStencilView->descriptorHeap = descriptorHeap;
	depthStencilView->descriptorHandle = descriptorHandle;
	if (resourceDescription.Format == DXGI_FORMAT_D24_UNORM_S8_UINT)
	{
		depthStencilView->textureFormat = TextureFormat::d24UnormS8Uint;
	}
	else if (resourceDescription.Format == DXGI_FORMAT_D32_FLOAT)
	{
		depthStencilView->textureFormat = TextureFormat::d32Float;
	}
	return depthStencilView.release();
}

void Dx12RenderBackend::destroyDepthStencilView(DepthStencilView* depthStencilView)
{
	delete depthStencilView;
}

void Dx12RenderBackend::queueRenderTargetViewForDestroy(RenderTargetView* renderTargetView)
{
	if (renderTargetView == nullptr)
	{
		return;
	}

	queuedRenderTargetViews.push_back(renderTargetView);
}

void Dx12RenderBackend::finalizeQueuedSubmissions()
{
	for (uint32 commandListIndex = 0; commandListIndex < queuedCommandLists.size(); ++commandListIndex)
	{
		releaseCommandList(queuedCommandLists[commandListIndex]);
	}
	queuedCommandLists.clear();

	if (commandQueue != nullptr)
	{
		commandQueue->clearQueued();
	}
}

void Dx12RenderBackend::releaseQueuedRenderResources()
{
	for (uint32 renderTargetViewIndex = 0; renderTargetViewIndex < queuedRenderTargetViews.size(); ++renderTargetViewIndex)
	{
		destroyRenderTargetView(queuedRenderTargetViews[renderTargetViewIndex]);
	}
	queuedRenderTargetViews.clear();
}

bool Dx12RenderBackend::reportDebugErrorsIfAny()
{
	if (device == nullptr)
	{
		return false;
	}

	com_pointer<ID3D12InfoQueue> infoQueue;
	if (FAILED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))) || infoQueue == nullptr)
	{
		return false;
	}

	const uint64 messageCount = static_cast<uint64>(infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter());
	if (messageCount == 0)
	{
		return false;
	}

	bool hasFailureMessage = false;
	for (uint64 messageIndex = 0; messageIndex < messageCount; ++messageIndex)
	{
		SIZE_T messageByteSize = 0;
		if (FAILED(infoQueue->GetMessage(static_cast<SIZE_T>(messageIndex), nullptr, &messageByteSize))
			|| messageByteSize == 0)
		{
			continue;
		}

		vector<char> messageStorage(messageByteSize);
		D3D12_MESSAGE* message = reinterpret_cast<D3D12_MESSAGE*>(messageStorage.data());
		if (FAILED(infoQueue->GetMessage(static_cast<SIZE_T>(messageIndex), message, &messageByteSize))
			|| message == nullptr)
		{
			continue;
		}

		const bool isFailureMessage = isDx12FailureSeverity(message->Severity);
		hasFailureMessage = hasFailureMessage || isFailureMessage;
		output_stream& selectedStream = isFailureMessage ? error : output;
		selectedStream << "[Dx12Debug][" << getDx12MessageSeverityText(message->Severity) << "] id="
					   << static_cast<uint32>(message->ID)
					   << " text="
					   << (message->pDescription != nullptr ? message->pDescription : "no_description")
					   << lineBreak;
	}

	infoQueue->ClearStoredMessages();
	return hasFailureMessage;
}

void Dx12RenderBackend::beginFrame(CommandList& commandList)
{
	if (pendingFrameTimestampFenceValue != 0)
	{
		if (frameTimestampFence == nullptr || frameTimestampReadbackBuffer == nullptr || frameTimestampFrequency == 0)
		{
			gpuFrameTimeMilliseconds = 0.0f;
			pendingFrameTimestampFenceValue = 0;
		}
		else if (frameTimestampFence->GetCompletedValue() >= pendingFrameTimestampFenceValue)
		{
			void* mappedData = nullptr;
			D3D12_RANGE readRange = { 0, sizeof(uint64) * 2u };
			if (FAILED(frameTimestampReadbackBuffer->Map(0, &readRange, &mappedData)) || mappedData == nullptr)
			{
				gpuFrameTimeMilliseconds = 0.0f;
			}
			else
			{
				const uint64* timestampData = static_cast<const uint64*>(mappedData);
				const uint64 beginTimestamp = timestampData[0];
				const uint64 endTimestamp = timestampData[1];
				if (endTimestamp > beginTimestamp)
				{
					const double elapsedGpuFrameTimeSeconds = static_cast<double>(endTimestamp - beginTimestamp) / static_cast<double>(frameTimestampFrequency);
					gpuFrameTimeMilliseconds = static_cast<float>(elapsedGpuFrameTimeSeconds * 1000.0);
				}
				else
				{
					gpuFrameTimeMilliseconds = 0.0f;
				}

				D3D12_RANGE writeRange = { 0, 0 };
				frameTimestampReadbackBuffer->Unmap(0, &writeRange);
			}

			pendingFrameTimestampFenceValue = 0;
		}
	}

	if (frameTimestampQueryHeap == nullptr
		|| frameTimestampReadbackBuffer == nullptr
		|| frameTimestampFence == nullptr
		|| pendingFrameTimestampFenceValue != 0
		|| frameTimestampFrequency == 0)
	{
		frameGpuTimingActive = false;
		return;
	}

	Dx12CommandList* dx12CommandList = dynamic_cast<Dx12CommandList*>(&commandList);
	if (dx12CommandList == nullptr || dx12CommandList->getNativeCommandList() == nullptr)
	{
		frameGpuTimingActive = false;
		return;
	}

	dx12CommandList->getNativeCommandList()->EndQuery(
		frameTimestampQueryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		0);
	frameGpuTimingActive = true;
}

void Dx12RenderBackend::endFrame(CommandList& commandList)
{
	if (!frameGpuTimingActive
		|| frameTimestampQueryHeap == nullptr
		|| frameTimestampReadbackBuffer == nullptr
		|| frameTimestampFrequency == 0)
	{
		frameGpuTimingActive = false;
		return;
	}

	Dx12CommandList* dx12CommandList = dynamic_cast<Dx12CommandList*>(&commandList);
	if (dx12CommandList == nullptr || dx12CommandList->getNativeCommandList() == nullptr)
	{
		frameGpuTimingActive = false;
		return;
	}

	ID3D12GraphicsCommandList* nativeCommandList = dx12CommandList->getNativeCommandList();
	nativeCommandList->EndQuery(
		frameTimestampQueryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		1);
	nativeCommandList->ResolveQueryData(
		frameTimestampQueryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		0,
		2,
		frameTimestampReadbackBuffer.Get(),
		0);
	frameGpuTimingActive = false;
	pendingFrameTimestampFenceValue = nextFrameTimestampFenceValue;
}

float Dx12RenderBackend::getGpuFrameTimeMilliseconds() const
{
	return gpuFrameTimeMilliseconds;
}

HandleWindow Dx12RenderBackend::getWindowHandle() const
{
	return windowHandle;
}

void* Dx12RenderBackend::getNativeGraphicsDevice()
{
	return device.Get();
}

void* Dx12RenderBackend::getNativeGraphicsFactory()
{
	return dxgiFactory.Get();
}

void* Dx12RenderBackend::getNativeGraphicsCommandQueue()
{
	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(commandQueue.get());
	if (dx12CommandQueue == nullptr)
	{
		return nullptr;
	}

	return dx12CommandQueue->getNativeCommandQueue();
}

bool Dx12RenderBackend::createDevice()
{
	const RenderBackendCreateOptions& createOptions = getCreateOptions();
	windowHandle = createOptions.windowHandle;
	if (windowHandle == nullptr)
	{
		return false;
	}

	if (!createFactory(createOptions.enableDebugLayer))
	{
		return false;
	}

	com_pointer<IDXGIAdapter1> selectedAdapter;
	for (uint32 adapterIndex = 0;; ++adapterIndex)
	{
		com_pointer<IDXGIAdapter1> adapter;
		if (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) == DXGI_ERROR_NOT_FOUND)
		{
			break;
		}

		DXGI_ADAPTER_DESC1 adapterDescription = {};
		adapter->GetDesc1(&adapterDescription);
		if ((adapterDescription.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
		{
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
		{
			selectedAdapter = adapter;
			break;
		}
	}

	if (selectedAdapter != nullptr)
	{
		return SUCCEEDED(D3D12CreateDevice(
			selectedAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&device)));
	}

	return SUCCEEDED(D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&device)));
}

bool Dx12RenderBackend::createCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC queueDescription = {};
	queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDescription.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDescription.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDescription.NodeMask = 0;

	com_pointer<ID3D12CommandQueue> createdCommandQueue;
	if (FAILED(device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&createdCommandQueue))))
	{
		return false;
	}

	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(commandQueue.get());
	if (dx12CommandQueue == nullptr)
	{
		return false;
	}

	dx12CommandQueue->setNativeCommandQueue(createdCommandQueue);
	frameTimestampFrequency = 0;
	if (FAILED(createdCommandQueue->GetTimestampFrequency(&frameTimestampFrequency)) || frameTimestampFrequency == 0)
	{
		frameTimestampFrequency = 0;
	}
	return true;
}

bool Dx12RenderBackend::createSwapChain(uint32 width, uint32 height)
{
	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(swapChain.get());
	if (dx12SwapChain == nullptr)
	{
		return false;
	}

	return dx12SwapChain->initialize(*this, width, height);
}

bool Dx12RenderBackend::createBackendResources()
{
	if (!createCommandResources())
	{
		return false;
	}

	frameTimestampQueryHeap.Reset();
	frameTimestampReadbackBuffer.Reset();
	frameTimestampFence.Reset();
	frameGpuTimingActive = false;
	nextFrameTimestampFenceValue = 1;
	pendingFrameTimestampFenceValue = 0;
	gpuFrameTimeMilliseconds = 0.0f;
	assert(device != nullptr && "[Dx12RenderBackend][Assert] reason=device_missing_during_backend_resource_create");

	D3D12_QUERY_HEAP_DESC queryHeapDescription = {};
	queryHeapDescription.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	queryHeapDescription.Count = 2;
	queryHeapDescription.NodeMask = 0;
	if (FAILED(device->CreateQueryHeap(&queryHeapDescription, IID_PPV_ARGS(&frameTimestampQueryHeap))))
	{
		frameTimestampQueryHeap.Reset();
		frameTimestampReadbackBuffer.Reset();
		return true;
	}

	D3D12_HEAP_PROPERTIES readbackHeapProperties = {};
	readbackHeapProperties.Type = D3D12_HEAP_TYPE_READBACK;
	readbackHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	readbackHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	readbackHeapProperties.CreationNodeMask = 1;
	readbackHeapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC readbackResourceDescription = {};
	readbackResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	readbackResourceDescription.Alignment = 0;
	readbackResourceDescription.Width = sizeof(uint64) * 2u;
	readbackResourceDescription.Height = 1;
	readbackResourceDescription.DepthOrArraySize = 1;
	readbackResourceDescription.MipLevels = 1;
	readbackResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
	readbackResourceDescription.SampleDesc.Count = 1;
	readbackResourceDescription.SampleDesc.Quality = 0;
	readbackResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	readbackResourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;
	if (FAILED(device->CreateCommittedResource(
		&readbackHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&readbackResourceDescription,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&frameTimestampReadbackBuffer))))
	{
		frameTimestampQueryHeap.Reset();
		frameTimestampReadbackBuffer.Reset();
		frameTimestampFence.Reset();
		return true;
	}

	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frameTimestampFence))))
	{
		frameTimestampQueryHeap.Reset();
		frameTimestampReadbackBuffer.Reset();
		frameTimestampFence.Reset();
	}
	return true;
}

void Dx12RenderBackend::destroyBackendResources()
{
	frameTimestampQueryHeap.Reset();
	frameTimestampReadbackBuffer.Reset();
	frameTimestampFence.Reset();
	frameGpuTimingActive = false;
	nextFrameTimestampFenceValue = 1;
	pendingFrameTimestampFenceValue = 0;
	gpuFrameTimeMilliseconds = 0.0f;
	clearPipelineStateObjects();
	clearRootSignatureObjects();

	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListPool.size(); ++commandListIndex)
	{
		Dx12CommandList* dx12CommandList = graphicsCommandListPool[commandListIndex].get();
		if (dx12CommandList != nullptr)
		{
			dx12CommandList->shutdown();
		}
	}

	resetCommandListPoolUsage();
}

void Dx12RenderBackend::destroySyncObject()
{
	Dx12SyncObject* dx12SyncObject = static_cast<Dx12SyncObject*>(syncObject.get());
	if (dx12SyncObject != nullptr)
	{
		dx12SyncObject->shutdown();
	}
}

void Dx12RenderBackend::destroySwapChain()
{
	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(swapChain.get());
	if (dx12SwapChain != nullptr)
	{
		dx12SwapChain->shutdown();
	}
}

void Dx12RenderBackend::destroyCommandQueue()
{
	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(commandQueue.get());
	if (dx12CommandQueue != nullptr)
	{
		dx12CommandQueue->setNativeCommandQueue(nullptr);
	}
}

void Dx12RenderBackend::destroyDevice()
{
	device.Reset();
	dxgiFactory.Reset();
	windowHandle = nullptr;
	frameTimestampFrequency = 0;
}

void Dx12RenderBackend::beforeDestroy()
{
	if (!isCreated())
	{
		return;
	}

	waitForGpuIdle();
	finalizeQueuedSubmissions();
	releaseQueuedRenderResources();
}

bool Dx12RenderBackend::createFactory(const bool enableDebugLayer)
{
	uint32 factoryFlags = 0;
	if (enableDebugLayer)
	{
		com_pointer<ID3D12Debug> debugController;
		const bool validDebugController = SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))
			&& debugController != nullptr;
		assert(validDebugController && "[BackendValidation][Assert] reason=debug_layer_unavailable");

		debugController->EnableDebugLayer();
		factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}

	return SUCCEEDED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&dxgiFactory)));
}

bool Dx12RenderBackend::createCommandResources()
{
	CommandListInitializeOptions initializeOptions = {};
	initializeOptions.renderBackend = this;
	initializeOptions.commandListType = CommandListType::graphics;

	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListPool.size(); ++commandListIndex)
	{
		Dx12CommandList* dx12CommandList = graphicsCommandListPool[commandListIndex].get();
		if (dx12CommandList == nullptr)
		{
			return false;
		}

		if (!dx12CommandList->initialize(initializeOptions))
		{
			return false;
		}
	}

	resetCommandListPoolUsage();
	return true;
}

bool Dx12RenderBackend::waitForGpuIdle()
{
	Dx12SyncObject* dx12SyncObject = static_cast<Dx12SyncObject*>(syncObject.get());
	if (dx12SyncObject == nullptr)
	{
		return true;
	}

	return dx12SyncObject->waitForGpuIdle();
}

void Dx12RenderBackend::resetCommandListPoolUsage()
{
	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListInUse.size(); ++commandListIndex)
	{
		graphicsCommandListInUse[commandListIndex] = false;
	}
}
