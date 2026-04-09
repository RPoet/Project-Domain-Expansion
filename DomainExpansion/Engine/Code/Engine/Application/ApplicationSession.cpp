#include "Engine/Application/ApplicationSession.h"

#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/Input/InputModule.h"
#include "Engine/Module/Profiler/ProfilerModule.h"
#include "Engine/Module/UI/ImGuiLayerModule.h"

WindowCreateOptions ApplicationSession::buildWindowCreateOptions() const
{
	WindowCreateOptions windowCreateOptions = {
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
	windowEventCallbacks.onNativeMessage = [this](
		const HandleWindow windowHandle,
		const MessageIdentifier messageIdentifier,
		const MessageFirstParameter firstParameter,
		const MessageSecondParameter secondParameter) -> bool
	{
		shared_pointer<InputModule> inputModule = InputModule::get();
		assert(inputModule != nullptr && "[ApplicationSession][Assert] reason=input_module_missing");
		inputModule->handleNativeMessage(windowHandle, messageIdentifier, firstParameter, secondParameter);
		if (!framework.isEditorUIEnabled())
		{
			return false;
		}

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
	frameworkInitializeOptions.backendOptions.forceDebugLayer = runOptions.forceBackendDebugLayer;
	frameworkInitializeOptions.backendOptions.validationInjectMode = runOptions.backendValidationInjectMode;
	frameworkInitializeOptions.profilerOptions.backendType = runOptions.profilerBackendType;
	frameworkInitializeOptions.profilerOptions.startupCaptureOutputFilePath = runOptions.profilerCaptureOutputFilePath;
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

void ApplicationSession::finishStartupCaptureIfNeeded() const
{
	shared_pointer<ProfilerModule> profilerModule = ProfilerModule::get();
	if (profilerModule == nullptr || !profilerModule->isCaptureActive())
	{
		return;
	}

	ProfilerCaptureResult captureResult = {};
	const bool endedCapture = profilerModule->endCapture(captureResult);
	assert(endedCapture && "[ApplicationSession][Assert] reason=startup_capture_end_failed");
	if (captureResult.outputFilePath.empty())
	{
		return;
	}

	output << "[Profiler][CaptureSaved] trace=" << captureResult.outputFilePath;
	if (!captureResult.xmlSummaryFilePath.empty())
	{
		output << " xmlSummary=" << captureResult.xmlSummaryFilePath;
	}

	output << lineBreak;
}

void ApplicationSession::updateFrame()
{
	framework.update();

	RenderWorldUpdateInput renderWorldUpdateInput = {
		.worldUpdateSerial = framework.getWorldUpdateSerial(),
		.renderCommandFlushInput = { .clearOnly = false },
	};
	renderWorld.update(renderWorldUpdateInput);

	const RenderWorldUpdateResult& renderWorldUpdateResult = renderWorld.getUpdateResult();
	framework.setRenderFramePerformanceMetrics(
		renderWorldUpdateResult.renderWorldCpuFrameTimeMilliseconds,
		renderWorldUpdateResult.renderCommandCpuFrameTimeMilliseconds,
		renderWorldUpdateResult.gpuFrameTimeMilliseconds);
}

int32 ApplicationSession::shutdownWithExitCode(const int32 exitCode)
{
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

	const WindowCreateOptions windowCreateOptions = buildWindowCreateOptions();
	const bool createdMainWindow = windowObject.create(windowCreateOptions);
	assert(createdMainWindow && "[ApplicationSession][Assert] reason=main_window_create_failed");
	windowObject.setEventCallbacks(buildWindowEventCallbacks());
	savePersistedWindowResolution();

	const FrameworkInitializeOptions frameworkInitializeOptions = buildFrameworkInitializeOptions();
	framework.registerModule(frameworkInitializeOptions);
	frameworkInitialized = framework.initialize(windowObject, frameworkInitializeOptions);
	if (!frameworkInitialized)
	{
		return shutdownWithExitCode(resolveFrameworkInitializeExitCode(
			framework.getRuntimeExitCode(),
			ExitCode::frameworkInitializeFailed));
	}

	renderWorldInitialized = renderWorld.initialize(windowObject);
	if (!renderWorldInitialized)
	{
		return shutdownWithExitCode(resolveFrameworkInitializeExitCode(
			framework.getRuntimeExitCode(),
			ExitCode::renderWorldInitializeFailed));
	}

	finishStartupCaptureIfNeeded();

	uint32 renderedFrameCount = 0;
	while (windowObject.pumpMessages())
	{
		updateFrame();
		++renderedFrameCount;
		if (runOptions.quitAfterFrameCount > 0 && renderedFrameCount >= runOptions.quitAfterFrameCount)
		{
			break;
		}

		Sleep(1);
	}

	return shutdownWithExitCode(static_cast<int32>(framework.getRuntimeExitCode()));
}
