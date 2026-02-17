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

	if (device == nullptr || commandQueue == nullptr || frameBufferCount == 0)
	{
		return false;
	}

	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frameFence))))
	{
		shutdown();
		return false;
	}

	frameFenceValues.resize(frameBufferCount);
	for (uint32 frameIndex = 0; frameIndex < frameFenceValues.size(); ++frameIndex)
	{
		frameFenceValues[frameIndex] = 0;
	}

	frameFenceEvent = CreateEventW(nullptr, boolFalse, boolFalse, nullptr);
	if (frameFenceEvent == nullptr)
	{
		shutdown();
		return false;
	}

	this->commandQueue = commandQueue;
	this->swapChain = swapChain;
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

	frameFenceValues.clear();
	commandQueue = nullptr;
	swapChain = nullptr;
	nextFenceValue = 1;
}

bool Dx12SyncObject::waitForGpuIdle()
{
	if (
		commandQueue == nullptr ||
		commandQueue->getNativeCommandQueue() == nullptr ||
		frameFence == nullptr ||
		frameFenceEvent == nullptr ||
		frameFenceValues.empty())
	{
		return true;
	}

	const uint64 signalValue = nextFenceValue;
	nextFenceValue += 1;
	if (FAILED(commandQueue->getNativeCommandQueue()->Signal(frameFence.Get(), signalValue)))
	{
		return false;
	}

	uint32 frameIndex = 0;
	if (swapChain != nullptr && swapChain->isRenderable())
	{
		frameIndex = swapChain->getCurrentImageIndex();
		if (frameIndex >= frameFenceValues.size())
		{
			frameIndex = 0;
		}
	}

	frameFenceValues[frameIndex] = signalValue;
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
	if (
		swapChain == nullptr ||
		!swapChain->isRenderable() ||
		frameFence == nullptr ||
		frameFenceEvent == nullptr ||
		frameFenceValues.empty())
	{
		return;
	}

	uint32 frameIndex = swapChain->getCurrentImageIndex();
	if (frameIndex >= frameFenceValues.size())
	{
		return;
	}

	if (frameFence->GetCompletedValue() >= frameFenceValues[frameIndex])
	{
		return;
	}

	if (FAILED(frameFence->SetEventOnCompletion(
		frameFenceValues[frameIndex],
		frameFenceEvent)))
	{
		return;
	}

	WaitForSingleObject(frameFenceEvent, INFINITE);
}

void Dx12SyncObject::signal()
{
	if (
		swapChain == nullptr ||
		!swapChain->isRenderable() ||
		commandQueue == nullptr ||
		commandQueue->getNativeCommandQueue() == nullptr ||
		frameFence == nullptr ||
		frameFenceValues.empty())
	{
		return;
	}

	uint32 frameIndex = swapChain->getCurrentImageIndex();
	if (frameIndex >= frameFenceValues.size())
	{
		return;
	}

	const uint64 signalValue = nextFenceValue;
	nextFenceValue += 1;
	if (FAILED(commandQueue->getNativeCommandQueue()->Signal(frameFence.Get(), signalValue)))
	{
		return;
	}

	frameFenceValues[frameIndex] = signalValue;
}

