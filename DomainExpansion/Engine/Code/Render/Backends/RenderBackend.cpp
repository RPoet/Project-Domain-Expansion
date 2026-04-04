#include "Render/Backends/RenderBackend.h"
#include "Render/Backends/Dx12/Dx12RenderBackend.h"

bool RenderBackend::create(const RenderBackendCreateOptions& options)
{
	destroy();

	if (!isSupportedBackend(options.backendType))
	{
		return false;
	}

	createOptions = options;
	if (!createDevice())
	{
		destroy();
		return false;
	}

	if (!createCommandQueue())
	{
		destroy();
		return false;
	}

	if (!createSwapChain(options.width, options.height))
	{
		destroy();
		return false;
	}

	if (!createBackendResources())
	{
		destroy();
		return false;
	}

	syncObject = createSyncObject();
	if (syncObject == nullptr)
	{
		destroy();
		return false;
	}

	createdState = true;
	return true;
}

void RenderBackend::destroy()
{
	beforeDestroy();
	destroyBackendResources();
	destroySyncObject();
	destroySwapChain();
	destroyCommandQueue();
	destroyDevice();

	createOptions = {};
	createdState = false;
}

bool RenderBackend::isSupportedBackend(const RenderBackendType backendType)
{
	return backendType == RenderBackendType::dx12;
}

unique_pointer<RenderBackend> RenderBackend::createBackend(const RenderBackendType backendType)
{
	if (!isSupportedBackend(backendType))
	{
		return nullptr;
	}

	if (backendType == RenderBackendType::dx12)
	{
		return unique_pointer<RenderBackend>(new Dx12RenderBackend());
	}

	return nullptr;
}

const RenderBackendCreateOptions& RenderBackend::getCreateOptions() const
{
	return createOptions;
}

bool RenderBackend::isCreated() const
{
	return createdState;
}

SyncObject* RenderBackend::getSyncObject()
{
	return syncObject.get();
}

void RenderBackend::beforeDestroy()
{
}
