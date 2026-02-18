#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include "Render/Backends/RenderBackend.h"

class Dx12CommandList;
class Dx12CommandQueue;
class Dx12SwapChain;
class Dx12SyncObject;

class Dx12RenderBackend final : public RenderBackend
{
public:
	Dx12RenderBackend();
	~Dx12RenderBackend() override = default;

	bool resize(uint32 width, uint32 height) override;
	CommandList* acquireCommandList() override;
	void releaseCommandList(CommandList* commandList) override;
	CommandQueue* getCommandQueue() override;
	SwapChain* getSwapChain() override;
	SyncObject* getSyncObject() override;
	RenderTargetView* createRenderTargetView(ResourceObject* resourceObject) override;
	void destroyRenderTargetView(RenderTargetView* renderTargetView) override;
	HandleWindow getWindowHandle() const override;
	uint32 getBackBufferWidth() const override;
	uint32 getBackBufferHeight() const override;
	uint32 getBackBufferCount() const override;
	void* getNativeGraphicsDevice() override;
	void* getNativeGraphicsFactory() override;
	void* getNativeGraphicsCommandQueue() override;

protected:
	bool createDevice() override;
	bool createCommandQueue() override;
	bool createSwapChain(uint32 width, uint32 height) override;
	bool createSyncObject() override;
	bool createBackendResources() override;
	void destroyBackendResources() override;
	void destroySyncObject() override;
	void destroySwapChain() override;
	void destroyCommandQueue() override;
	void destroyDevice() override;
	void beforeDestroy() override;

private:
	bool createFactory(bool enableDebugLayer);
	bool createBackBufferViewHeap();
	bool createCommandResources();
	void destroyBackBufferViewHeap();
	bool waitForGpuIdle();
	void resetCommandListPoolUsage();

	static constexpr uint32 frameBufferCount = 2;
	static constexpr uint32 graphicsCommandListPoolCapacity = 4;

	HandleWindow windowHandle = nullptr;

	com_pointer<IDXGIFactory6> dxgiFactory;
	com_pointer<ID3D12Device> device;

	// TO DO : Make Descriptor heap pool, this view heap is just for speed up purpose.
	com_pointer<ID3D12DescriptorHeap> backBufferViewHeap;
	vector<unique_pointer<Dx12CommandList>> graphicsCommandListPool;
	vector<bool> graphicsCommandListInUse;
	unique_pointer<CommandQueue> commandQueue;
	unique_pointer<SwapChain> swapChain;
	unique_pointer<SyncObject> syncObject;
	uint32 backBufferViewDescriptorSize = 0;
};
