#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include "Render/Backends/RenderBackend.h"
#include "Render/Backends/Dx12/Dx12PipelineStateDesc.h"
#include "Render/Backends/Dx12/Dx12DepthStencilView.h"
#include "Render/Backends/Dx12/Dx12PipelineStateObject.h"
#include "Render/Backends/Dx12/Dx12RootSignatureObject.h"
#include "Render/Backends/Dx12/Dx12RootSignatureDesc.h"
#include "Render/PipelineStateManager.h"
#include "Render/RootSignatureManager.h"

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
	CommandList* acquireCommandList(CommandListType commandListType) override;
	bool supportsCommandListType(CommandListType commandListType) const override;
	void releaseCommandList(CommandList* commandList) override;
	void queueCommandList(CommandList* commandList) override;
	void executeQueuedCommandLists() override;
	CommandQueue* getCommandQueue() override;
	SwapChain* getSwapChain() override;
	unique_pointer<SyncObject> createSyncObject() override;
	unique_pointer<BufferResourceObject> createBufferObject(const BufferObjectCreateOptions& createOptions) override;
	unique_pointer<TextureResourceObject> createTextureObject(const TextureObjectCreateOptions& createOptions) override;
	RootSignatureObject* getOrCreateRootSignatureObject(const RootSignatureDesc& rootSignatureDesc) override;
	PipelineStateObject* getOrCreatePipelineStateObject(const PipelineStateDesc& pipelineStateDesc) override;
	void clearRootSignatureObjects() override;
	void clearPipelineStateObjects() override;
	RenderTargetView* createRenderTargetView(TextureResourceObject* textureResourceObject) override;
	void destroyRenderTargetView(RenderTargetView* renderTargetView) override;
	DepthStencilView* createDepthStencilView(TextureResourceObject* textureResourceObject) override;
	void destroyDepthStencilView(DepthStencilView* depthStencilView) override;
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
	RootSignatureManager<Dx12RootSignatureDesc, Dx12RootSignatureObject> rootSignatureManager;
	PipelineStateManager<Dx12PipelineStateDesc, Dx12PipelineStateObject> pipelineStateManager;
	vector<CommandList*> queuedCommandLists;
	vector<RenderTargetView*> queuedRenderTargetViews;
	unique_pointer<CommandQueue> commandQueue;
	unique_pointer<SwapChain> swapChain;
};
