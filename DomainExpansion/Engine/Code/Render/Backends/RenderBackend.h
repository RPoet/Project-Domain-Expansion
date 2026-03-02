#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/CommandList.h"
#include "Render/CommandQueue.h"
#include "Render/RenderTargetView.h"
#include "Render/RootSignatureObject.h"
#include "Render/ResourceObject.h"
#include "Render/SwapChain.h"
#include "Render/SyncObject.h"

enum class RenderBackendType : uint32
{
	dx12 = 0,
	vulkan = 1,
	metal = 2,
};

struct RenderBackendCreateOptions
{
	HandleWindow windowHandle = nullptr;
	uint32 width = 0;
	uint32 height = 0;
	RenderBackendType backendType = RenderBackendType::dx12;
	bool enableDebugLayer = false;
};

class RenderBackend
{
public:
	virtual ~RenderBackend() = default;
	RenderBackend(const RenderBackend&) = delete;
	RenderBackend& operator=(const RenderBackend&) = delete;
	RenderBackend(RenderBackend&&) = delete;
	RenderBackend& operator=(RenderBackend&&) = delete;

	bool create(const RenderBackendCreateOptions& options);
	void destroy();
	virtual CommandList* acquireCommandList() = 0;
	virtual CommandList* acquireCommandList(CommandListType commandListType)
	{
		unused(commandListType);
		return acquireCommandList();
	}
	virtual bool supportsCommandListType(CommandListType commandListType) const = 0;
	virtual void releaseCommandList(CommandList* commandList) = 0;
	virtual void queueCommandList(CommandList* commandList) = 0;
	virtual void executeQueuedCommandLists() = 0;
	virtual CommandQueue* getCommandQueue() = 0;
	virtual SwapChain* getSwapChain() = 0;
	virtual SyncObject* getSyncObject() = 0;
	virtual unique_pointer<BufferResourceObject> createBufferObject(const BufferObjectCreateOptions& createOptions) = 0;
	virtual RootSignatureObject* getOrCreateRootSignatureObject(const RootSignatureDesc& rootSignatureDesc) = 0;
	virtual void clearRootSignatureObjects() = 0;
	virtual RenderTargetView* createRenderTargetView(ResourceObject* resourceObject) = 0;
	virtual void destroyRenderTargetView(RenderTargetView* renderTargetView) = 0;
	virtual void queueRenderTargetViewForDestroy(RenderTargetView* renderTargetView) = 0;
	virtual void releaseQueuedRenderResources() = 0;
	virtual bool reportDebugErrorsIfAny() = 0;
	virtual HandleWindow getWindowHandle() const = 0;
	virtual void* getNativeGraphicsDevice() = 0;
	virtual void* getNativeGraphicsFactory() = 0;
	virtual void* getNativeGraphicsCommandQueue() = 0;

	static bool isSupportedBackend(RenderBackendType backendType);
	static unique_pointer<RenderBackend> createBackend(RenderBackendType backendType);

protected:
	RenderBackend() = default;
	const RenderBackendCreateOptions& getCreateOptions() const;
	bool isCreated() const;
	virtual bool createDevice() = 0;
	virtual bool createCommandQueue() = 0;
	virtual bool createSwapChain(uint32 width, uint32 height) = 0;
	virtual bool createSyncObject() = 0;
	virtual bool createBackendResources() = 0;
	virtual void destroyBackendResources() = 0;
	virtual void destroySyncObject() = 0;
	virtual void destroySwapChain() = 0;
	virtual void destroyCommandQueue() = 0;
	virtual void destroyDevice() = 0;
	virtual void beforeDestroy();

private:
	RenderBackendCreateOptions createOptions = {};
	bool createdState = false;
};
