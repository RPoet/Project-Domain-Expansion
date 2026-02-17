#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/CommandList.h"
#include "Render/CommandQueue.h"
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
	virtual bool resize(uint32 width, uint32 height) = 0;
	virtual CommandList* acquireCommandList() = 0;
	virtual void releaseCommandList(CommandList* commandList) = 0;
	virtual CommandQueue* getPrimaryCommandQueue() = 0;
	virtual SwapChain* getPrimarySwapChain() = 0;
	virtual SyncObject* getPrimarySyncObject() = 0;
	virtual HandleWindow getWindowHandle() const = 0;
	virtual uint32 getBackBufferWidth() const = 0;
	virtual uint32 getBackBufferHeight() const = 0;
	virtual uint32 getBackBufferCount() const = 0;
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
