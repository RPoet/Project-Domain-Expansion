#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Framework/Framework.h"

bool RenderBackendModule::init(Framework& framework)
{
	destroyBackend();

	if (framework.getExecutionFlow() != FrameworkExecutionFlow::backendFlow)
	{
		return true;
	}

	WindowsWindowObject* windowObject = framework.getWindowObject();
	if (windowObject == nullptr)
	{
		error << "[BackendCLI][Error] stage=create reason=window_object_missing" << lineBreak;
		return false;
	}

	const FrameworkBackendOptions& backendOptions = framework.getBackendOptions();
	if (!RenderBackend::isSupportedBackend(backendOptions.backendType))
	{
		error << "[BackendCLI][Error] stage=create reason=backend_not_supported" << lineBreak;
		return false;
	}

	RenderBackendCreateOptions backendCreateOptions = {};
	backendCreateOptions.windowHandle = windowObject->getWindowHandle();
	backendCreateOptions.width = windowObject->getClientWidth();
	backendCreateOptions.height = windowObject->getClientHeight();
	backendCreateOptions.backendType = backendOptions.backendType;
	backendCreateOptions.enableDebugLayer = backendOptions.enableDebugLayer;

	if (!createBackend(backendCreateOptions))
	{
		error << "[BackendCLI][Error] stage=create reason=backend_create_failed" << lineBreak;
		return false;
	}

	return true;
}

void RenderBackendModule::update()
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

bool RenderBackendModule::resizeBackend(const uint32 width, const uint32 height)
{
	if (renderBackend == nullptr)
	{
		return false;
	}

	return renderBackend->resize(width, height);
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
