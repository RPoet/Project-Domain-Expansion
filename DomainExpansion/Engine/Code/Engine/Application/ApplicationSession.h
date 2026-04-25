#pragma once

#include "Engine/Application/ApplicationRunOptions.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Window/WindowsWindowObject.h"
#include "Render/RenderWorld.h"

class ApplicationSession final
{
public:
	int32 run(const ApplicationRunOptions& applicationRunOptions);

private:
	enum class ExitCode : int32
	{
		success = 0,
		frameworkInitializeFailed = -2,
		renderWorldInitializeFailed = -3,
	};

	WindowCreateOptions buildWindowCreateOptions() const;
	WindowEventCallbacks buildWindowEventCallbacks();
	FrameworkInitializeOptions buildFrameworkInitializeOptions() const;
	void openConsoleWindowIfNeeded() const;
	void loadPersistedWindowResolution(WindowCreateOptions& windowCreateOptions) const;
	void savePersistedWindowResolution() const;
	void finishStartupCaptureIfNeeded() const;
	void updateFrame();
	int32 shutdownWithExitCode(int32 exitCode);

	ApplicationRunOptions runOptions = {};
	WindowsWindowObject windowObject = {};
	Framework framework = {};
	RenderWorld renderWorld = {};
	bool frameworkInitialized = false;
	bool renderWorldInitialized = false;
};
