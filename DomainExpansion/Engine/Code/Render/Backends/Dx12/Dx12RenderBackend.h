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
	void finalizeQueuedSubmissions() override;
	void releaseQueuedRenderResources() override;
	bool reportDebugErrorsIfAny() override;
	void beginFrame(CommandList& commandList) override;
	void endFrame(CommandList& commandList) override;
	float getGpuFrameTimeMilliseconds() const override;
	HandleWindow getWindowHandle() const override;
	void* getNativeGraphicsDevice() override;
	void* getNativeGraphicsFactory() override;
	void* getNativeGraphicsCommandQueue() override;

protected:
	// TO DO : move all this initialize stuff into common interface like init().
	bool createDevice() override;
	bool createCommandQueue() override;
	bool createSwapChain(uint32 width, uint32 height) override;
	bool createBackendResources() override;

	// TO DO : move all this destroy stuff into common interface like teardown().
	void destroyBackendResources() override;
	void destroySyncObject() override;
	void destroySwapChain() override;
	void destroyCommandQueue() override;
	void destroyDevice() override;
	void beforeDestroy() override;

private:
	// TO DO : move all this initialize stuff into common interface like init().
	bool createFactory(bool enableDebugLayer);
	bool createCommandResources();

	// TO DO : this can be common interface
	bool waitForGpuIdle();

	void resetCommandListPoolUsage();

	static constexpr uint32 graphicsCommandListPoolCapacity = 4;

	HandleWindow windowHandle = nullptr;

	com_pointer<IDXGIFactory6> dxgiFactory;
	com_pointer<ID3D12Device> device;
	com_pointer<ID3D12QueryHeap> frameTimestampQueryHeap;
	com_pointer<ID3D12Resource> frameTimestampReadbackBuffer;
	com_pointer<ID3D12Fence> frameTimestampFence;
	uint64 frameTimestampFrequency = 0;
	uint64 nextFrameTimestampFenceValue = 1;
	uint64 pendingFrameTimestampFenceValue = 0;
	bool frameGpuTimingActive = false;
	float gpuFrameTimeMilliseconds = 0.0f;

	vector<unique_pointer<Dx12CommandList>> graphicsCommandListPool;
	vector<bool> graphicsCommandListInUse;
	RootSignatureManager<Dx12RootSignatureDesc, Dx12RootSignatureObject> rootSignatureManager;
	PipelineStateManager<Dx12PipelineStateDesc, Dx12PipelineStateObject> pipelineStateManager;
	vector<CommandList*> queuedCommandLists;
	vector<RenderTargetView*> queuedRenderTargetViews;
	unique_pointer<CommandQueue> commandQueue;
	unique_pointer<SwapChain> swapChain;
};
