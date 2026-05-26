#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Framework/Framework.h"

bool RenderBackendModule::initialize(Framework& framework)
{
	destroyBackend();

	const FrameworkBackendOptions& backendOptions = framework.getBackendOptions();
	if (!backendOptions.createBackend)
	{
		return true;
	}

	WindowsWindowObject* windowObject = framework.getWindowObject();
	const bool validWindowObject = windowObject != nullptr;
	assert(validWindowObject && "[RenderBackendModule][Assert] reason=window_object_missing");

	const bool supportedBackend = RenderBackend::isSupportedBackend(backendOptions.backendType);
	assert(supportedBackend && "[RenderBackendModule][Assert] reason=backend_not_supported");

	RenderBackendCreateOptions backendCreateOptions = {};
	backendCreateOptions.windowHandle = windowObject->getWindowHandle();
	backendCreateOptions.width = windowObject->getClientWidth();
	backendCreateOptions.height = windowObject->getClientHeight();
	backendCreateOptions.backendType = backendOptions.backendType;
	backendCreateOptions.enableDebugLayer = backendOptions.enableDebugLayer;
	const bool createdBackend = createBackend(backendCreateOptions);
	assert(createdBackend && "[RenderBackendModule][Assert] reason=backend_create_failed");
	return createdBackend;
}

void RenderBackendModule::preUpdate()
{
}

void RenderBackendModule::postUpdate()
{
}

void RenderBackendModule::shutdown()
{
	destroyBackend();
}

bool RenderBackendModule::createBackend(const RenderBackendCreateOptions& createOptions)
{
	destroyBackend();

	if (!RenderBackend::isSupportedBackend(createOptions.backendType))
	{
		return false;
	}

	renderBackend = RenderBackend::createBackend(createOptions.backendType);
	if (renderBackend == nullptr)
	{
		return false;
	}

	if (!renderBackend->create(createOptions))
	{
		destroyBackend();
		return false;
	}

	return true;
}

void RenderBackendModule::destroyBackend()
{
	if (renderBackend != nullptr)
	{
		renderBackend->destroy();
		renderBackend.reset();
	}
}

RenderBackend* RenderBackendModule::getBackend()
{
	return renderBackend.get();
}

const RenderBackend* RenderBackendModule::getBackend() const
{
	return renderBackend.get();
}

bool RenderBackendModule::isBackendCreated() const
{
	return renderBackend != nullptr;
}
