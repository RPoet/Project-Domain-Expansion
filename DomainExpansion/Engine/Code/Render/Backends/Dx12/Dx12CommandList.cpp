#include "Render/Backends/Dx12/Dx12CommandList.h"
#include "Render/Backends/Dx12/Dx12SwapChain.h"
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

bool Dx12CommandList::initialize(com_pointer<ID3D12Device> device, const uint32 frameBufferCount)
{
	shutdown();

	if (device == nullptr || frameBufferCount == 0)
	{
		return false;
	}

	this->frameBufferCount = frameBufferCount;
	commandAllocators.resize(frameBufferCount);
	for (uint32 frameIndex = 0; frameIndex < frameBufferCount; ++frameIndex)
	{
		if (FAILED(device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&commandAllocators[frameIndex]))))
		{
			shutdown();
			return false;
		}
	}

	if (FAILED(device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocators[0].Get(),
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
	for (uint32 frameIndex = 0; frameIndex < commandAllocators.size(); ++frameIndex)
	{
		commandAllocators[frameIndex].Reset();
	}

	commandAllocators.clear();
	frameBufferCount = 0;
	activeFrameIndex = 0;
	recordingAvailable = false;
	swapChain = nullptr;
}

void Dx12CommandList::setSwapChain(Dx12SwapChain* swapChain)
{
	this->swapChain = swapChain;
}

void Dx12CommandList::beginRecord()
{
	recordingAvailable = false;

	if (
		commandList == nullptr ||
		swapChain == nullptr ||
		!swapChain->isRenderable())
	{
		return;
	}

	activeFrameIndex = swapChain->getCurrentImageIndex();
	if (
		activeFrameIndex >= frameBufferCount ||
		activeFrameIndex >= commandAllocators.size() ||
		commandAllocators[activeFrameIndex] == nullptr)
	{
		return;
	}

	if (FAILED(commandAllocators[activeFrameIndex]->Reset()))
	{
		return;
	}

	if (FAILED(commandList->Reset(
		commandAllocators[activeFrameIndex].Get(),
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
	if (
		commandList == nullptr ||
		!recordingAvailable ||
		resourceObject == nullptr)
	{
		return;
	}

	Dx12ResourceObject* dx12ResourceObject = static_cast<Dx12ResourceObject*>(resourceObject);
	if (dx12ResourceObject->resource == nullptr)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER transitionBarrier = {};
	transitionBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	transitionBarrier.Transition.pResource = dx12ResourceObject->resource.Get();
	transitionBarrier.Transition.StateBefore = getDx12ResourceState(beforeState);
	transitionBarrier.Transition.StateAfter = getDx12ResourceState(afterState);
	transitionBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &transitionBarrier);
}

void Dx12CommandList::setRenderTarget(RenderTargetView* renderTargetView)
{
	if (
		commandList == nullptr ||
		!recordingAvailable ||
		renderTargetView == nullptr)
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
	if (
		commandList == nullptr ||
		!recordingAvailable ||
		renderTargetView == nullptr)
	{
		return;
	}

	Dx12RenderTargetView* dx12RenderTargetView = static_cast<Dx12RenderTargetView*>(renderTargetView);
	const float clearColor[4] = { red, green, blue, alpha };
	commandList->ClearRenderTargetView(dx12RenderTargetView->descriptorHandle, clearColor, 0, nullptr);
}

void Dx12CommandList::flush()
{
	if (
		commandList == nullptr ||
		!recordingAvailable)
	{
		return;
	}

	commandList->Close();
}

ID3D12GraphicsCommandList* Dx12CommandList::getNativeCommandList() const
{
	return commandList.Get();
}
