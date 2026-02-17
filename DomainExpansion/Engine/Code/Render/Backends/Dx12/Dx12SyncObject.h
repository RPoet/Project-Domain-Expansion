#pragma once

#include <d3d12.h>
#include "Render/SyncObject.h"

class Dx12CommandQueue;
class Dx12SwapChain;

class Dx12SyncObject final : public SyncObject
{
public:
	Dx12SyncObject() = default;
	bool initialize(
		com_pointer<ID3D12Device> device,
		Dx12CommandQueue* commandQueue,
		Dx12SwapChain* swapChain,
		uint32 frameBufferCount);
	void shutdown();
	bool waitForGpuIdle();

	void wait() override;
	void signal() override;

private:
	Dx12CommandQueue* commandQueue = nullptr;
	Dx12SwapChain* swapChain = nullptr;
	com_pointer<ID3D12Fence> frameFence;
	HandleEvent frameFenceEvent = nullptr;
	vector<uint64> frameFenceValues;
	uint64 nextFenceValue = 1;
};
