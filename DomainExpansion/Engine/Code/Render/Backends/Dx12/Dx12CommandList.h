#pragma once

#include <d3d12.h>
#include "Render/CommandList.h"

class Dx12SwapChain;

class Dx12CommandList final : public CommandList
{
public:
	Dx12CommandList() = default;
	bool initialize(com_pointer<ID3D12Device> device, uint32 frameBufferCount);
	void shutdown();
	void setSwapChain(Dx12SwapChain* swapChain);

	void beginRecord() override;
	void resourceBarrier(
		ResourceObject* resourceObject,
		ResourceState beforeState,
		ResourceState afterState) override;
	void setRenderTarget(RenderTargetView* renderTargetView) override;
	void clearRenderTarget(
		RenderTargetView* renderTargetView,
		float red,
		float green,
		float blue,
		float alpha) override;
	void flush() override;
	ID3D12GraphicsCommandList* getNativeCommandList() const;

private:
	vector<com_pointer<ID3D12CommandAllocator>> commandAllocators;
	com_pointer<ID3D12GraphicsCommandList> commandList;
	Dx12SwapChain* swapChain = nullptr;
	uint32 frameBufferCount = 0;
	uint32 activeFrameIndex = 0;
	bool recordingAvailable = false;
};
