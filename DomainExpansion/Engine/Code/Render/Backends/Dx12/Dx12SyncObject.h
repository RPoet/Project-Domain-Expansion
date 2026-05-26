#pragma once

#include <d3d12.h>
#include "Render/Backends/SyncObject.h"

class Dx12CommandQueue;
class Dx12SwapChain;

class Dx12SyncObject final : public SyncObject
{
public:
	Dx12SyncObject() = default;
	bool initialize(com_pointer<ID3D12Device> device, Dx12CommandQueue* commandQueue);
	void shutdown();
	bool waitForGpuIdle();

	void wait() override;
	void wait(uint64 targetSyncValue) override;
	uint64 signal() override;
	uint64 getCompletedSyncValue() const override final;

private:
	Dx12CommandQueue* commandQueue = nullptr;
	com_pointer<ID3D12Fence> frameFence;
	HandleEvent frameFenceEvent = nullptr;
	uint64 lastSubmittedFenceValue = 0;
	uint64 nextFenceValue = 1;
};
