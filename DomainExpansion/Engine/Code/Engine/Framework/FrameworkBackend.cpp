#include "Engine/Framework/Framework.h"
#include "Engine/Module/Render/RenderBackendModule.h"

void Framework::onWindowResize(const uint32 width, const uint32 height)
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	if (renderBackendModule == nullptr || !renderBackendModule->isBackendCreated())
	{
		return;
	}

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	if (renderBackend == nullptr)
	{
		return;
	}

	SyncObject* syncObject = renderBackend->getSyncObject();
	if (syncObject != nullptr)
	{
		syncObject->wait();
	}

	SwapChain* swapChain = renderBackend->getSwapChain();
	const bool resizeSucceeded = swapChain != nullptr && swapChain->resize(width, height);
	assert(resizeSucceeded && "[Framework][Assert] reason=render_backend_resize_failed");
}
