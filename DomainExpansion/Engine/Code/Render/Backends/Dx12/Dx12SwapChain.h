#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include "Render/SwapChain.h"

class RenderBackend;

class Dx12SwapChain final : public SwapChain
{
public:
	Dx12SwapChain() = default;

	bool initialize(RenderBackend& renderBackend);
	bool resize(uint32 width, uint32 height);
	void shutdown();

	bool isRenderable() const override;
	uint32 getCurrentImageIndex() const override;
	ResourceObject* getCurrentBackBufferResource() override;
	RenderTargetView* getCurrentBackBufferView() override;
	void present() override;

private:
	bool createBackBufferResources();
	void releaseBackBufferResources();

	HandleWindow windowHandle = nullptr;
	com_pointer<ID3D12Device> device;
	com_pointer<IDXGISwapChain4> swapChain;
	com_pointer<ID3D12DescriptorHeap> backBufferViewHeap;
	vector<unique_pointer<ResourceObject>> backBufferResources;
	vector<unique_pointer<RenderTargetView>> backBufferViews;
	uint32 frameBufferCount = 0;
	uint32 backBufferViewDescriptorSize = 0;
	uint32 backBufferWidth = 0;
	uint32 backBufferHeight = 0;
};
