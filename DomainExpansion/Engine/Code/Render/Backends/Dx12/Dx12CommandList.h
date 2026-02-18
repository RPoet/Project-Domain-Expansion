#pragma once

#include <d3d12.h>
#include "Render/CommandList.h"

class Dx12CommandList final : public CommandList
{
public:
	Dx12CommandList() = default;
	bool initialize(const CommandListInitializeOptions& initializeOptions) override;
	void shutdown() override;

	void reset() override;
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
	void close() override;
	ID3D12GraphicsCommandList* getNativeCommandList() const;

private:
	com_pointer<ID3D12CommandAllocator> commandAllocator;
	com_pointer<ID3D12GraphicsCommandList> commandList;
	bool recordingAvailable = false;
};
