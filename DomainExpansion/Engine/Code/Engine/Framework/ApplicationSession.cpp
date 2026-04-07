#include "Engine/Framework/ApplicationSession.h"

#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/Input/InputModule.h"
#include "Engine/Module/UI/ImGuiLayerModule.h"

WindowCreateOptions ApplicationSession::buildWindowCreateOptions() const
{
	WindowCreateOptions windowCreateOptions{
		.windowTitle = L"DomainExpansion Engine",
		.initialClientWidth = 1600,
		.initialClientHeight = 900,
		.startVisible = true,
		.startBorderlessFullscreen = false,
	};
	loadPersistedWindowResolution(windowCreateOptions);
	return windowCreateOptions;
}

WindowEventCallbacks ApplicationSession::buildWindowEventCallbacks()
{
	WindowEventCallbacks windowEventCallbacks = {};
	windowEventCallbacks.onResize = [this](const uint32 width, const uint32 height)
	{
		shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
		assert(diskLoaderModule != nullptr && "[ApplicationSession][Assert] reason=disk_loader_module_missing");
		diskLoaderModule->TEMP_saveRuntimeWindowResolution(width, height);
		framework.onWindowResize(width, height);
	};
	windowEventCallbacks.onActivationChanged = [](const bool isActive)
	{
		output << "Window activation changed: " << (isActive ? "active" : "inactive") << lineBreak;
	};
	windowEventCallbacks.onNativeMessage = [](
		const HandleWindow windowHandle,
		const MessageIdentifier messageIdentifier,
		const MessageFirstParameter firstParameter,
		const MessageSecondParameter secondParameter) -> bool
	{
		shared_pointer<InputModule> inputModule = InputModule::get();
		assert(inputModule != nullptr && "[ApplicationSession][Assert] reason=input_module_missing");
		inputModule->handleNativeMessage(windowHandle, messageIdentifier, firstParameter, secondParameter);

		shared_pointer<ImGuiLayerModule> imGuiLayerModule = ImGuiLayerModule::get();
		assert(imGuiLayerModule != nullptr && "[ApplicationSession][Assert] reason=imgui_layer_module_missing");
		return imGuiLayerModule->processNativeMessage(windowHandle, messageIdentifier, firstParameter, secondParameter);
	};
	return windowEventCallbacks;
}

FrameworkInitializeOptions ApplicationSession::buildFrameworkInitializeOptions() const
{
	FrameworkInitializeOptions frameworkInitializeOptions = {};
	frameworkInitializeOptions.backendOptions.backendType = runOptions.backendType;
	frameworkInitializeOptions.backendOptions.enableDebugLayer = runOptions.enableBackendDebugLayer;
	frameworkInitializeOptions.backendOptions.validationInjectMode = runOptions.backendValidationInjectMode;
	return frameworkInitializeOptions;
}

void ApplicationSession::loadPersistedWindowResolution(WindowCreateOptions& windowCreateOptions) const
{
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[ApplicationSession][Assert] reason=disk_loader_module_missing");
	uint32 runtimeWindowWidth = 0;
	uint32 runtimeWindowHeight = 0;
	if (!diskLoaderModule->TEMP_loadRuntimeWindowResolution(runtimeWindowWidth, runtimeWindowHeight))
	{
		return;
	}

	windowCreateOptions.initialClientWidth = static_cast<int32>(runtimeWindowWidth);
	windowCreateOptions.initialClientHeight = static_cast<int32>(runtimeWindowHeight);
}

void ApplicationSession::savePersistedWindowResolution() const
{
	if (windowObject.getWindowHandle() == nullptr)
	{
		return;
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[ApplicationSession][Assert] reason=disk_loader_module_missing");
	diskLoaderModule->TEMP_saveRuntimeWindowResolution(windowObject.getClientWidth(), windowObject.getClientHeight());
}

void ApplicationSession::updateFrame()
{
	TRACE_EVENT("startup", "ApplicationSession::Frame");
	framework.update();

	RenderWorldUpdateInput renderWorldUpdateInput{
		.worldUpdateSerial = framework.getWorldUpdateSerial(),
		.renderCommandFlushInput { .clearOnly = false }
	};
	renderWorld.update(renderWorldUpdateInput);

	const RenderWorldUpdateResult& renderWorldUpdateResult = renderWorld.getUpdateResult();
	framework.setRenderFramePerformanceMetrics(
		renderWorldUpdateResult.renderWorldCpuFrameTimeMilliseconds,
		renderWorldUpdateResult.renderCommandCpuFrameTimeMilliseconds,
		renderWorldUpdateResult.gpuFrameTimeMilliseconds);
	perfettoCapture.onFramePresentedAndStopIfNeeded();
}

int32 ApplicationSession::shutdownWithExitCode(const int32 exitCode)
{
	perfettoCapture.endIfActive();

	if (renderWorldInitialized)
	{
		renderWorld.shutdown();
		renderWorldInitialized = false;
	}

	if (frameworkInitialized)
	{
		framework.shutdown();
		frameworkInitialized = false;
	}

	savePersistedWindowResolution();
	windowObject.destroy();
	return exitCode;
}

int32 ApplicationSession::run(const ApplicationRunOptions& applicationRunOptions)
{
	runOptions = applicationRunOptions;
	perfettoCapture.beginStartupCapture(runOptions.perfettoStartupCapture);

	const WindowCreateOptions windowCreateOptions = buildWindowCreateOptions();
	const bool createdMainWindow = windowObject.create(windowCreateOptions);
	assert(createdMainWindow && "[ApplicationSession][Assert] reason=main_window_create_failed");
	windowObject.setEventCallbacks(buildWindowEventCallbacks());
	savePersistedWindowResolution();

	const FrameworkInitializeOptions frameworkInitializeOptions = buildFrameworkInitializeOptions();
	framework.registerModule(frameworkInitializeOptions);
	{
		TRACE_EVENT("startup", "ApplicationSession::Startup");
		frameworkInitialized = framework.initialize(windowObject, frameworkInitializeOptions);
		if (frameworkInitialized)
		{
			renderWorldInitialized = renderWorld.initialize(windowObject);
		}
	}

	if (!frameworkInitialized)
	{
		return shutdownWithExitCode(resolveFrameworkInitializeExitCode(framework.getRuntimeExitCode(), ExitCode::frameworkInitializeFailed));
	}

	if (!renderWorldInitialized)
	{
		return shutdownWithExitCode(resolveFrameworkInitializeExitCode(framework.getRuntimeExitCode(), ExitCode::renderWorldInitializeFailed));
	}

	while (windowObject.pumpMessages())
	{
		updateFrame();
		Sleep(1);
	}

	return shutdownWithExitCode(static_cast<int32>(framework.getRuntimeExitCode()));
}
