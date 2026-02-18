#include "Render/Backends/Dx12/Dx12CommandList.h"
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

bool Dx12CommandList::initialize(const CommandListInitializeOptions& initializeOptions)
{
	shutdown();

	ID3D12Device* device = static_cast<ID3D12Device*>(initializeOptions.nativeGraphicsDevice);
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
	if (commandList == nullptr
		|| !recordingAvailable
		|| resourceObject == nullptr)
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
	if (commandList == nullptr
		|| !recordingAvailable
		|| renderTargetView == nullptr)
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
	if (commandList == nullptr
		|| !recordingAvailable
		|| renderTargetView == nullptr)
	{
		return;
	}

	Dx12RenderTargetView* dx12RenderTargetView = static_cast<Dx12RenderTargetView*>(renderTargetView);
	const float clearColor[4] = { red, green, blue, alpha };
	commandList->ClearRenderTargetView(dx12RenderTargetView->descriptorHandle, clearColor, 0, nullptr);
}

void Dx12CommandList::close()
{
	if (commandList == nullptr
		|| !recordingAvailable)
	{
		return;
	}

	commandList->Close();
}

ID3D12GraphicsCommandList* Dx12CommandList::getNativeCommandList() const
{
	return commandList.Get();
}
