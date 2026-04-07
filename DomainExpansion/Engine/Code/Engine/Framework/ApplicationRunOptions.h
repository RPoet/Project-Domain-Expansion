#pragma once

#include "Engine/Framework/BackendValidation.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Engine/Profiler/PerfettoTrace.h"
#include "Render/Backends/RenderBackendDefinitions.h"

struct ApplicationRunOptions
{
	RenderBackendType backendType = RenderBackendType::dx12;
#if defined(_DEBUG)
	bool enableBackendDebugLayer = true;
#else
	bool enableBackendDebugLayer = false;
#endif
	BackendValidationInjectMode backendValidationInjectMode = BackendValidationInjectMode::none;
	PerfettoStartupCaptureRequest perfettoStartupCapture = {};
};

ApplicationRunOptions parseApplicationRunOptions(WideStringPointer commandLine);
