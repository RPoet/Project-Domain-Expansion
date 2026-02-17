#include "Engine/Framework/Framework.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Render/RenderCommand.h"

static const char* getBackendTypeText(const RenderBackendType backendType)
{
	switch (backendType)
	{
	case RenderBackendType::dx12:
		return "dx12";
	case RenderBackendType::vulkan:
		return "vulkan";
	case RenderBackendType::metal:
		return "metal";
	default:
		return "unknown";
	}
}

bool Framework::initializeBackendFlow()
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();

	output << "[BackendCLI][Begin] mode=backend api="
		   << getBackendTypeText(backendOptions.backendType)
		   << " frames=" << backendOptions.frameCount << lineBreak;

	if (renderBackendModule == nullptr)
	{
		error << "[BackendCLI][Error] stage=create reason=backend_module_missing" << lineBreak;
		runtimeExitCode = 3;
		executionCompleted = true;
		return false;
	}

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	if (renderBackend == nullptr)
	{
		error << "[BackendCLI][Error] stage=create reason=backend_not_initialized_by_module" << lineBreak;
		runtimeExitCode = 3;
		executionCompleted = true;
		return false;
	}

	CommandList* initialCommandList = renderBackend->acquireCommandList();
	const bool rendererBindingFailed =
		initialCommandList == nullptr ||
		renderBackend->getPrimaryCommandQueue() == nullptr ||
		renderBackend->getPrimarySyncObject() == nullptr;
	if (initialCommandList != nullptr)
	{
		renderBackend->releaseCommandList(initialCommandList);
	}

	if (rendererBindingFailed)
	{
		error << "[BackendCLI][Error] stage=create reason=renderer_bind_failed" << lineBreak;
		renderBackendModule->destroyBackend();
		runtimeExitCode = 3;
		executionCompleted = true;
		return false;
	}

	renderer.setBackend(renderBackend);
	if (!screen.initialize(*renderBackend))
	{
		error << "[BackendCLI][Error] stage=create reason=screen_initialize_failed" << lineBreak;
		renderBackendModule->destroyBackend();
		renderer.setBackend(nullptr);
		screen.shutdown();
		runtimeExitCode = 3;
		executionCompleted = true;
		return false;
	}

	backendCreated = true;
	backendFinalizePending = false;
	output << "[BackendCLI][Create] device=ok swapchain=ok" << lineBreak;
	return true;
}

void Framework::onWindowResize(const uint32 width, const uint32 height)
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();

	if (executionFlow != FrameworkExecutionFlow::backendFlow)
	{
		output << "Window resized to " << width << "x" << height << lineBreak;
		return;
	}

	if (!backendCreated || renderBackendModule == nullptr || !renderBackendModule->isBackendCreated())
	{
		return;
	}

	if (renderBackendModule->resizeBackend(width, height))
	{
		++backendResizeCount;
		output << "[BackendCLI][Resize] width=" << width
			   << " height=" << height
			   << " status=ok" << lineBreak;
		return;
	}

	backendResizeFailed = true;
	error << "[BackendCLI][Error] stage=resize reason=resize_failed" << lineBreak;
}

bool Framework::tickBackendFlow(const float deltaTimeSeconds)
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();

	if (
		windowsWindowObject == nullptr ||
		renderBackendModule == nullptr ||
		!renderBackendModule->isBackendCreated() ||
		!backendCreated)
	{
		runtimeExitCode = 3;
		executionCompleted = true;
		return false;
	}

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	if (renderBackend == nullptr)
	{
		runtimeExitCode = 3;
		executionCompleted = true;
		return false;
	}

	if (windowsWindowObject->isWindowMinimized())
	{
		return true;
	}

	if (backendOptions.forceResize && !backendForcedResizeSubmitted && renderedBackendFrameCount == 10)
	{
		const uint32 forcedWidth = windowsWindowObject->getClientWidth() + 32;
		const uint32 forcedHeight = windowsWindowObject->getClientHeight() + 32;
		SetWindowPos(
			windowsWindowObject->getWindowHandle(),
			nullptr,
			0,
			0,
			static_cast<int32>(forcedWidth),
			static_cast<int32>(forcedHeight),
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		backendForcedResizeSubmitted = true;
	}

	if (backendResizeFailed)
	{
		finalizeBackendFlow(false);
		return false;
	}

	unused(deltaTimeSeconds);
	CommandList* commandList = renderBackend->acquireCommandList();
	if (commandList == nullptr)
	{
		error << "[BackendCLI][Error] stage=render reason=command_list_acquire_failed" << lineBreak;
		finalizeBackendFlow(false);
		return false;
	}

	RenderCommand& renderCommand = RenderCommand::get();
	renderCommand.enqueue("renderer.render", [this, commandList](const string& commandName, RenderBackend& renderBackendReference)
	{
		unused(commandName);
		unused(renderBackendReference);
		renderer.render(commandList);
	});
	renderCommand.enqueue("screen.present", [this](const string& commandName, RenderBackend& renderBackendReference)
	{
		unused(commandName);
		if (!screen.isRenderable())
		{
			return;
		}

		SyncObject* syncObject = renderBackendReference.getPrimarySyncObject();
		if (syncObject == nullptr)
		{
			return;
		}

		screen.present();
		syncObject->signal();
	});
	renderCommand.enqueue("backend.releaseCommandList", [commandList](const string& commandName, RenderBackend& renderBackendReference)
	{
		unused(commandName);
		renderBackendReference.releaseCommandList(commandList);
	});

	output << "[BackendCLI][Frame] index=" << renderedBackendFrameCount << " present=ok" << lineBreak;
	++renderedBackendFrameCount;

	if (renderedBackendFrameCount >= backendOptions.frameCount)
	{
		backendFinalizePending = true;
	}

	return true;
}

void Framework::finalizeBackendFlow(const bool passState)
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();

	if (executionCompleted)
	{
		return;
	}

	const bool backendPass =
		passState &&
		!backendResizeFailed &&
		(renderedBackendFrameCount == backendOptions.frameCount);
	output << "[BackendCLI][Summary] frameCount=" << renderedBackendFrameCount
		   << " resizeCount=" << backendResizeCount
		   << " result=" << (backendPass ? "pass" : "fail") << lineBreak;

	if (renderBackendModule != nullptr)
	{
		renderBackendModule->destroyBackend();
	}

	renderer.setBackend(nullptr);
	screen.shutdown();
	backendCreated = false;
	backendFinalizePending = false;
	executionCompleted = true;
	runtimeExitCode = backendPass ? 0 : 4;
}
