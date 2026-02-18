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

	CommandList* acquireCommandList() override;
	void releaseCommandList(CommandList* commandList) override;
	void queueCommandList(CommandList* commandList) override;
	void executeQueuedCommandLists() override;
	CommandQueue* getCommandQueue() override;
	SwapChain* getSwapChain() override;
	SyncObject* getSyncObject() override;
	RenderTargetView* createRenderTargetView(ResourceObject* resourceObject) override;
	void destroyRenderTargetView(RenderTargetView* renderTargetView) override;
	void queueRenderTargetViewForDestroy(RenderTargetView* renderTargetView) override;
	void releaseQueuedRenderResources() override;
	bool reportDebugErrorsIfAny() override;
	HandleWindow getWindowHandle() const override;
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
	bool createCommandResources();
	bool waitForGpuIdle();
	void resetCommandListPoolUsage();

	static constexpr uint32 graphicsCommandListPoolCapacity = 4;

	HandleWindow windowHandle = nullptr;

	com_pointer<IDXGIFactory6> dxgiFactory;
	com_pointer<ID3D12Device> device;

	vector<unique_pointer<Dx12CommandList>> graphicsCommandListPool;
	vector<bool> graphicsCommandListInUse;
	vector<CommandList*> queuedCommandLists;
	vector<RenderTargetView*> queuedRenderTargetViews;
	unique_pointer<CommandQueue> commandQueue;
	unique_pointer<SwapChain> swapChain;
	unique_pointer<SyncObject> syncObject;
};
