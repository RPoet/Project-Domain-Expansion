#include "Render/Backends/Dx12/Dx12CommandList.h"
#include "Render/Backends/Dx12/Dx12PipelineStateObject.h"
#include "Render/Backends/Dx12/Dx12RootSignatureObject.h"
#include "Render/Backends/RenderBackendDefinitions.h"
#include "Render/Backends/Dx12/Dx12Converter.h"
#include "Render/Backends/Dx12/Dx12DepthStencilView.h"
#include "Render/Backends/Dx12/Dx12RenderTargetView.h"
#include "Render/Backends/Dx12/Dx12ResourceObject.h"

static D3D_PRIMITIVE_TOPOLOGY getDx12PrimitiveTopology(const PrimitiveTopology primitiveTopology)
{
	switch (primitiveTopology)
	{
	case PrimitiveTopology::pointList:
		return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	case PrimitiveTopology::lineList:
		return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
	case PrimitiveTopology::triangleList:
	default:
		return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}
}

static DXGI_FORMAT getDx12IndexFormat(const IndexElementSize elementSize)
{
	switch (elementSize)
	{
	case IndexElementSize::sixteenBits:
		return DXGI_FORMAT_R16_UINT;
	case IndexElementSize::thirtyTwoBits:
	default:
		return DXGI_FORMAT_R32_UINT;
	}
}

bool Dx12CommandList::initialize(const CommandListInitializeOptions& initializeOptions)
{
	unused(initializeOptions);
	shutdown();
	return true;
}

void Dx12CommandList::shutdown()
{
	commandAllocator = nullptr;
	commandList = nullptr;
	recordingAvailable = false;
}

void Dx12CommandList::assignCommandAllocator(ID3D12CommandAllocator* commandAllocator)
{
	this->commandAllocator = commandAllocator;
	recordingAvailable = false;
}

ID3D12CommandAllocator* Dx12CommandList::detachCommandAllocator()
{
	ID3D12CommandAllocator* detachedCommandAllocator = commandAllocator;
	commandAllocator = nullptr;
	recordingAvailable = false;
	return detachedCommandAllocator;
}

void Dx12CommandList::assignCommandList(ID3D12GraphicsCommandList* commandList)
{
	this->commandList = commandList;
	recordingAvailable = false;
}

ID3D12GraphicsCommandList* Dx12CommandList::detachCommandList()
{
	ID3D12GraphicsCommandList* detachedCommandList = commandList;
	commandList = nullptr;
	recordingAvailable = false;
	return detachedCommandList;
}

void Dx12CommandList::reset()
{
	recordingAvailable = false;

	assert(commandList != nullptr && commandAllocator != nullptr);

	if (FAILED(commandAllocator->Reset()))
	{
		return;
	}

	if (FAILED(commandList->Reset(
		commandAllocator,
		nullptr)))
	{
		return;
	}

	recordingAvailable = true;
}

void Dx12CommandList::resourceBarrier(
	ResourceObject* resourceObject,
	const ResourceState beforeState,
	const ResourceState afterState)
{
	if (!isRecordingReady() || resourceObject == nullptr)
	{
		return;
	}

	ID3D12Resource* dx12Resource = static_cast<ID3D12Resource*>(resourceObject->getNativeResource());

	D3D12_RESOURCE_BARRIER transitionBarrier = {};
	transitionBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	transitionBarrier.Transition.pResource = dx12Resource;
	transitionBarrier.Transition.StateBefore = getDx12ResourceState(beforeState);
	transitionBarrier.Transition.StateAfter = getDx12ResourceState(afterState);
	transitionBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &transitionBarrier);
}

void Dx12CommandList::setRenderTargets(
	RenderTargetView* const* renderTargetViews,
	const uint32 renderTargetViewCount,
	DepthStencilView* depthStencilView)
{
	const bool hasRenderTargets = renderTargetViewCount > 0;
	const bool hasDepthStencil = depthStencilView != nullptr;
	if (!isRecordingReady()
		|| renderTargetViewCount > renderBackendRenderTargetSlotCount
		|| (!hasRenderTargets && !hasDepthStencil))
	{
		return;
	}

	assert(!hasRenderTargets || renderTargetViews != nullptr);

	D3D12_CPU_DESCRIPTOR_HANDLE dx12RenderTargetDescriptorHandles[renderBackendRenderTargetSlotCount] = {};
	for (uint32 renderTargetIndex = 0; renderTargetIndex < renderTargetViewCount; ++renderTargetIndex)
	{
		RenderTargetView* renderTargetView = renderTargetViews[renderTargetIndex];
		assert(renderTargetView != nullptr);

		Dx12RenderTargetView* dx12RenderTargetView = static_cast<Dx12RenderTargetView*>(renderTargetView);
		dx12RenderTargetDescriptorHandles[renderTargetIndex] = dx12RenderTargetView->descriptorHandle;
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE* depthStencilDescriptorHandle = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE dx12DepthStencilDescriptorHandle = {};
	if (depthStencilView != nullptr)
	{
		Dx12DepthStencilView* dx12DepthStencilView = static_cast<Dx12DepthStencilView*>(depthStencilView);
		dx12DepthStencilDescriptorHandle = dx12DepthStencilView->descriptorHandle;
		depthStencilDescriptorHandle = &dx12DepthStencilDescriptorHandle;
	}

	commandList->OMSetRenderTargets(
		renderTargetViewCount,
		renderTargetViewCount > 0 ? dx12RenderTargetDescriptorHandles : nullptr,
		boolFalse,
		depthStencilDescriptorHandle);
}

void Dx12CommandList::clearRenderTarget(
	RenderTargetView* renderTargetView,
	const float red,
	const float green,
	const float blue,
	const float alpha)
{
	if (!isRecordingReady() || renderTargetView == nullptr)
	{
		return;
	}

	Dx12RenderTargetView* dx12RenderTargetView = static_cast<Dx12RenderTargetView*>(renderTargetView);
	const float clearColor[4] = { red, green, blue, alpha };
	commandList->ClearRenderTargetView(dx12RenderTargetView->descriptorHandle, clearColor, 0, nullptr);
}

void Dx12CommandList::clearDepthStencil(
	DepthStencilView* depthStencilView,
	const float depthValue,
	const uint32 stencilValue)
{
	if (!isRecordingReady() || depthStencilView == nullptr)
	{
		return;
	}

	Dx12DepthStencilView* dx12DepthStencilView = static_cast<Dx12DepthStencilView*>(depthStencilView);
	D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH;
	if (dx12DepthStencilView->getTextureFormat() == TextureFormat::d24UnormS8Uint)
	{
		clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
	}

	commandList->ClearDepthStencilView(
		dx12DepthStencilView->descriptorHandle,
		clearFlags,
		depthValue,
		static_cast<UINT8>(stencilValue & 0xFFu),
		0,
		nullptr);
}

void Dx12CommandList::setViewport(const ViewportArea& viewportArea)
{
	if (!isRecordingReady())
	{
		return;
	}

	if (viewportArea.width <= 0.0f || viewportArea.height <= 0.0f)
	{
		return;
	}

	D3D12_VIEWPORT viewport = {};
	viewport.TopLeftX = viewportArea.topLeftX;
	viewport.TopLeftY = viewportArea.topLeftY;
	viewport.Width = viewportArea.width;
	viewport.Height = viewportArea.height;
	viewport.MinDepth = viewportArea.minDepth;
	viewport.MaxDepth = viewportArea.maxDepth;
	commandList->RSSetViewports(1, &viewport);
}

void Dx12CommandList::setScissorRect(const ScissorRectArea& scissorRectArea)
{
	if (!isRecordingReady())
	{
		return;
	}

	if (scissorRectArea.right <= scissorRectArea.left
		|| scissorRectArea.bottom <= scissorRectArea.top)
	{
		return;
	}

	D3D12_RECT scissorRect = {};
	scissorRect.left = scissorRectArea.left;
	scissorRect.top = scissorRectArea.top;
	scissorRect.right = scissorRectArea.right;
	scissorRect.bottom = scissorRectArea.bottom;
	commandList->RSSetScissorRects(1, &scissorRect);
}

void Dx12CommandList::setPrimitiveTopology(const PrimitiveTopology primitiveTopology)
{
	if (!isRecordingReady())
	{
		return;
	}

	commandList->IASetPrimitiveTopology(getDx12PrimitiveTopology(primitiveTopology));
}

void Dx12CommandList::setVertexBuffer(const uint32 slotIndex, const VertexBufferBinding& vertexBufferBinding)
{
	if (!isRecordingReady() || vertexBufferBinding.resourceObject == nullptr)
	{
		return;
	}

	ID3D12Resource* dx12Resource = static_cast<ID3D12Resource*>(vertexBufferBinding.resourceObject->getNativeResource());
	if (dx12Resource == nullptr)
	{
		return;
	}

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	vertexBufferView.BufferLocation = dx12Resource->GetGPUVirtualAddress() + vertexBufferBinding.offsetInBytes;
	vertexBufferView.StrideInBytes = vertexBufferBinding.strideInBytes;
	vertexBufferView.SizeInBytes = vertexBufferBinding.sizeInBytes;

	commandList->IASetVertexBuffers(slotIndex, 1, &vertexBufferView);
}

void Dx12CommandList::setIndexBuffer(const IndexBufferBinding& indexBufferBinding)
{
	if (!isRecordingReady() || indexBufferBinding.resourceObject == nullptr)
	{
		return;
	}

	ID3D12Resource* dx12Resource = static_cast<ID3D12Resource*>(indexBufferBinding.resourceObject->getNativeResource());
	if (dx12Resource == nullptr)
	{
		return;
	}

	D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
	indexBufferView.BufferLocation = dx12Resource->GetGPUVirtualAddress() + indexBufferBinding.offsetInBytes;
	indexBufferView.Format = getDx12IndexFormat(indexBufferBinding.elementSize);
	indexBufferView.SizeInBytes = indexBufferBinding.sizeInBytes;

	commandList->IASetIndexBuffer(&indexBufferView);
}

void Dx12CommandList::setPipeline(PipelineStateObject* pipelineStateObject, RootSignatureObject* rootSignatureObject)
{
	if (!isRecordingReady() || pipelineStateObject == nullptr || rootSignatureObject == nullptr)
	{
		assert(false && "[Dx12CommandList][Assert] reason=set_pipeline_precondition_failed");
	}

	Dx12PipelineStateObject* dx12PipelineStateObject = static_cast<Dx12PipelineStateObject*>(pipelineStateObject);
	Dx12RootSignatureObject* dx12RootSignatureObject = static_cast<Dx12RootSignatureObject*>(rootSignatureObject);
	ID3D12PipelineState* dx12PipelineState = dx12PipelineStateObject->getPipelineState().Get();
	ID3D12RootSignature* dx12RootSignature = dx12RootSignatureObject->getRootSignature().Get();
	if (dx12PipelineState == nullptr || dx12RootSignature == nullptr)
	{
		assert(false && "[Dx12CommandList][Assert] reason=set_pipeline_native_object_missing");
	}

	const uint64 pipelineRootSignatureHash = dx12PipelineStateObject->getPlatformPipelineStateDesc().rootSignatureHash;
	const uint64 boundRootSignatureHash = dx12RootSignatureObject->getPlatformRootSignatureDesc().getHashValue();
	if (pipelineRootSignatureHash != boundRootSignatureHash)
	{
		assert(false && "[Dx12CommandList][Assert] reason=set_pipeline_root_signature_mismatch");
	}

	const PipelineStateType pipelineStateType = dx12PipelineStateObject->getPlatformPipelineStateDesc().pipelineStateType;
	if (pipelineStateType == PipelineStateType::graphics)
	{
		commandList->SetGraphicsRootSignature(dx12RootSignature);
	}
	else if (pipelineStateType == PipelineStateType::compute)
	{
		commandList->SetComputeRootSignature(dx12RootSignature);
	}
	else
	{
		assert(false && "[Dx12CommandList][Assert] reason=pipeline_type_unsupported");
	}

	commandList->SetPipelineState(dx12PipelineState);
}

void Dx12CommandList::setGraphicsPushConstants(
	const uint32 pushConstantRangeIndex,
	const void* data,
	const uint32 sizeInBytes)
{
	if (!isRecordingReady() || data == nullptr || sizeInBytes == 0 || (sizeInBytes & 3u) != 0)
	{
		assert(false && "[Dx12CommandList][Assert] reason=set_graphics_push_constants_precondition_failed");
	}

	commandList->SetGraphicsRoot32BitConstants(
		pushConstantRangeIndex,
		sizeInBytes / 4u,
		data,
		0);
}

void Dx12CommandList::copyBuffer(
	BufferResourceObject* destinationBufferObject,
	const uint64 destinationOffsetInBytes,
	BufferResourceObject* sourceBufferObject,
	const uint64 sourceOffsetInBytes,
	const uint64 copySizeInBytes)
{
	if (!isRecordingReady()
		|| destinationBufferObject == nullptr
		|| sourceBufferObject == nullptr
		|| copySizeInBytes == 0)
	{
		return;
	}

	ID3D12Resource* destinationDx12Buffer = static_cast<ID3D12Resource*>(destinationBufferObject->getNativeResource());
	ID3D12Resource* sourceDx12Buffer = static_cast<ID3D12Resource*>(sourceBufferObject->getNativeResource());
	if (destinationDx12Buffer == nullptr || sourceDx12Buffer == nullptr)
	{
		return;
	}

	commandList->CopyBufferRegion(
		destinationDx12Buffer,
		destinationOffsetInBytes,
		sourceDx12Buffer,
		sourceOffsetInBytes,
		copySizeInBytes);
}

void Dx12CommandList::drawIndexed(
	const uint32 indexCountPerInstance,
	const uint32 instanceCount,
	const uint32 startIndexLocation,
	const int32 baseVertexLocation,
	const uint32 startInstanceLocation)
{
	if (!isRecordingReady() || indexCountPerInstance == 0 || instanceCount == 0)
	{
		return;
	}

	commandList->DrawIndexedInstanced(
		indexCountPerInstance,
		instanceCount,
		startIndexLocation,
		baseVertexLocation,
		startInstanceLocation);
}

void Dx12CommandList::draw(
	const uint32 vertexCountPerInstance,
	const uint32 instanceCount,
	const uint32 startVertexLocation,
	const uint32 startInstanceLocation)
{
	if (!isRecordingReady() || vertexCountPerInstance == 0 || instanceCount == 0)
	{
		return;
	}

	commandList->DrawInstanced(
		vertexCountPerInstance,
		instanceCount,
		startVertexLocation,
		startInstanceLocation);
}

void Dx12CommandList::close()
{
	if (!isRecordingReady())
	{
		return;
	}

	commandList->Close();
}

ID3D12GraphicsCommandList* Dx12CommandList::getNativeCommandList() const
{
	return commandList;
}

bool Dx12CommandList::isRecordingReady() const
{
	return commandList != nullptr && recordingAvailable;
}
