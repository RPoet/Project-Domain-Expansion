#include "Render/Backends/Dx12/Dx12CommandList.h"
#include "Render/Backends/RenderBackend.h"
#include "Render/Backends/Dx12/Dx12RenderTargetView.h"
#include "Render/Backends/Dx12/Dx12ResourceObject.h"

static D3D12_RESOURCE_STATES getDx12ResourceState(const ResourceState resourceState)
{
	switch (resourceState)
	{
	case ResourceState::present:
		return D3D12_RESOURCE_STATE_PRESENT;
	case ResourceState::renderTarget:
		return D3D12_RESOURCE_STATE_RENDER_TARGET;
	default:
		return D3D12_RESOURCE_STATE_COMMON;
	}
}

static D3D12_COMMAND_LIST_TYPE getDx12CommandListType(const CommandListType commandListType)
{
	switch (commandListType)
	{
	case CommandListType::graphics:
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
	case CommandListType::compute:
		return D3D12_COMMAND_LIST_TYPE_COMPUTE;
	case CommandListType::copy:
		return D3D12_COMMAND_LIST_TYPE_COPY;
	default:
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
	}
}

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
	shutdown();

	if (initializeOptions.renderBackend == nullptr)
	{
		return false;
	}

	ID3D12Device* device = static_cast<ID3D12Device*>(initializeOptions.renderBackend->getNativeGraphicsDevice());
	if (device == nullptr)
	{
		return false;
	}

	const D3D12_COMMAND_LIST_TYPE commandListType = getDx12CommandListType(initializeOptions.commandListType);
	if (FAILED(device->CreateCommandAllocator(
		commandListType,
		IID_PPV_ARGS(&commandAllocator))))
	{
		shutdown();
		return false;
	}

	if (FAILED(device->CreateCommandList(
		0,
		commandListType,
		commandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&commandList))))
	{
		shutdown();
		return false;
	}

	if (FAILED(commandList->Close()))
	{
		shutdown();
		return false;
	}

	return true;
}

void Dx12CommandList::shutdown()
{
	commandList.Reset();
	commandAllocator.Reset();
	recordingAvailable = false;
}

void Dx12CommandList::reset()
{
	recordingAvailable = false;

	if (commandList == nullptr || commandAllocator == nullptr)
	{
		return;
	}

	if (FAILED(commandAllocator->Reset()))
	{
		return;
	}

	if (FAILED(commandList->Reset(
		commandAllocator.Get(),
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

void Dx12CommandList::setRenderTarget(RenderTargetView* renderTargetView)
{
	if (!isRecordingReady() || renderTargetView == nullptr)
	{
		return;
	}

	Dx12RenderTargetView* dx12RenderTargetView = static_cast<Dx12RenderTargetView*>(renderTargetView);
	commandList->OMSetRenderTargets(1, &dx12RenderTargetView->descriptorHandle, boolFalse, nullptr);
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
	return commandList.Get();
}

bool Dx12CommandList::isRecordingReady() const
{
	return commandList != nullptr && recordingAvailable;
}
