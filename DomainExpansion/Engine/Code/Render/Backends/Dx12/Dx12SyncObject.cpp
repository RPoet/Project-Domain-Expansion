#include "Render/Backends/Dx12/Dx12SyncObject.h"
#include "Render/Backends/Dx12/Dx12CommandQueue.h"
#include "Render/Backends/Dx12/Dx12SwapChain.h"

bool Dx12SyncObject::initialize(
	com_pointer<ID3D12Device> device,
	Dx12CommandQueue* commandQueue,
	Dx12SwapChain* swapChain,
	const uint32 frameBufferCount)
{
	shutdown();

	unused(swapChain);
	if (device == nullptr || commandQueue == nullptr || frameBufferCount == 0)
	{
		return false;
	}

	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frameFence))))
	{
		shutdown();
		return false;
	}

	frameFenceEvent = CreateEventW(nullptr, boolFalse, boolFalse, nullptr);
	if (frameFenceEvent == nullptr)
	{
		shutdown();
		return false;
	}

	this->commandQueue = commandQueue;
	lastSubmittedFenceValue = 0;
	nextFenceValue = 1;
	return true;
}

void Dx12SyncObject::shutdown()
{
	frameFence.Reset();
	if (frameFenceEvent != nullptr)
	{
		CloseHandle(frameFenceEvent);
		frameFenceEvent = nullptr;
	}

	commandQueue = nullptr;
	lastSubmittedFenceValue = 0;
	nextFenceValue = 1;
}

bool Dx12SyncObject::waitForGpuIdle()
{
	if (commandQueue == nullptr
		|| commandQueue->getNativeCommandQueue() == nullptr
		|| frameFence == nullptr
		|| frameFenceEvent == nullptr)
	{
		return true;
	}

	const uint64 signalValue = nextFenceValue;
	nextFenceValue += 1;
	if (FAILED(commandQueue->getNativeCommandQueue()->Signal(frameFence.Get(), signalValue)))
	{
		return false;
	}

	lastSubmittedFenceValue = signalValue;
	if (frameFence->GetCompletedValue() >= signalValue)
	{
		return true;
	}

	if (FAILED(frameFence->SetEventOnCompletion(signalValue, frameFenceEvent)))
	{
		return false;
	}

	return WaitForSingleObject(frameFenceEvent, INFINITE) == WAIT_OBJECT_0;
}

void Dx12SyncObject::wait()
{
	if (frameFence == nullptr
		|| frameFenceEvent == nullptr
		|| lastSubmittedFenceValue == 0)
	{
		return;
	}

	if (frameFence->GetCompletedValue() >= lastSubmittedFenceValue)
	{
		return;
	}

	if (FAILED(frameFence->SetEventOnCompletion(
		lastSubmittedFenceValue,
		frameFenceEvent)))
	{
		return;
	}

	WaitForSingleObject(frameFenceEvent, INFINITE);
}

void Dx12SyncObject::signal()
{
	if (commandQueue == nullptr
		|| commandQueue->getNativeCommandQueue() == nullptr
		|| frameFence == nullptr)
	{
		return;
	}

	const uint64 signalValue = nextFenceValue;
	nextFenceValue += 1;
	if (FAILED(commandQueue->getNativeCommandQueue()->Signal(frameFence.Get(), signalValue)))
	{
		return;
	}

	lastSubmittedFenceValue = signalValue;
}

uint64 Dx12SyncObject::getCompletedSyncValue() const
{
	if (frameFence == nullptr)
	{
		return 0;
	}

	return frameFence->GetCompletedValue();
}
