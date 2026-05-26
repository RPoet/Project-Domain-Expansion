#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include "Render/Backends/ObjectPool.h"
#include "Render/Backends/RenderBackend.h"
#include "Render/Backends/Dx12/Dx12CommandList.h"
#include "Render/Backends/Dx12/Dx12PipelineStateDesc.h"
#include "Render/Backends/Dx12/Dx12DepthStencilView.h"
#include "Render/Backends/Dx12/Dx12PipelineStateObject.h"
#include "Render/Backends/Dx12/Dx12RootSignatureObject.h"
#include "Render/Backends/Dx12/Dx12RootSignatureDesc.h"
#include "Render/Backends/PipelineStateManager.h"
#include "Render/Backends/RootSignatureManager.h"

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
	unique_pointer<HeapObject> createHeapObject(const HeapObjectCreateOptions& createOptions) override;
	ResourceAllocationInfo getBufferObjectAllocationInfo(const BufferObjectCreateOptions& createOptions) override;
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
	void discardPendingSubmissionBatch() override;
	bool reportDebugErrorsIfAny() override;
	void beginFrame(CommandList& commandList) override;
	void endFrame(CommandList& commandList) override;
	float getGpuFrameTimeMilliseconds() const override;
	HandleWindow getWindowHandle() const override;
	void* getNativeGraphicsDevice() override;
	ID3D12Device4* getNativeGraphicsDevice4() const;
	void* getNativeGraphicsFactory() override;
	void* getNativeGraphicsCommandQueue() override;

protected:
	// TO DO : move all this initialize stuff into common interface like initialize().
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
	struct DeferredReuseBatch
	{
		vector<ID3D12CommandAllocator*> commandAllocators;
		vector<RenderTargetView*> renderTargetViews;
	};

	// TO DO : move all this initialize stuff into common interface like initialize().
	bool createFactory(bool enableDebugLayer);
	bool createCommandResources();
	ID3D12CommandList* createGraphicsCommandListObject(CommandListType commandListType);
	ID3D12CommandAllocator* createGraphicsCommandAllocator(CommandListType commandListType);
	void releaseDeferredReuseBatch(DeferredReuseBatch& deferredReuseBatch);

	// TO DO : this can be common interface
	bool waitForGpuIdle();

	static constexpr uint32 nativeCommandListPoolCapacity = 8;
	static constexpr uint32 nativeCommandAllocatorPoolCapacity = 12;
	static constexpr uint32 commandListPoolCapacity = 8;
	static constexpr uint32 maxPendingFrameCount = 2;
	static constexpr uint32 invalidFrameBatchIndex = 0xFFFFFFFFu;

	HandleWindow windowHandle = nullptr;

	com_pointer<IDXGIFactory6> dxgiFactory;
	com_pointer<ID3D12Device> device;
	com_pointer<ID3D12Device4> device4;
	com_pointer<ID3D12QueryHeap> frameTimestampQueryHeaps[maxPendingFrameCount] = {};
	com_pointer<ID3D12Resource> frameTimestampReadbackBuffers[maxPendingFrameCount] = {};
	bool frameTimestampReadbackPending[maxPendingFrameCount] = {};
	uint64 frameTimestampFrequency = 0;
	bool frameGpuTimingActive = false;
	float gpuFrameTimeMilliseconds = 0.0f;

	RootSignatureManager<Dx12RootSignatureDesc, Dx12RootSignatureObject> rootSignatureManager;
	PipelineStateManager<Dx12PipelineStateDesc, Dx12PipelineStateObject> pipelineStateManager;
	DeferredReuseBatch deferredReuseBatches[maxPendingFrameCount] = {};
	uint32 currentFrameBatchIndex = invalidFrameBatchIndex;
	uint32 nextFrameBatchIndex = 0;
	unique_pointer<Dx12CommandQueue> commandQueue;
	unique_pointer<Dx12SwapChain> swapChain;

	// TO DO : CommandList type must be considered when implements compute and copy
	ObjectPool<Dx12CommandList> commandListPool;
	ObjectPool<ID3D12CommandList> nativeCommandListPool;
	ObjectPool<ID3D12CommandAllocator> nativeCommandAllocatorPool;
};
