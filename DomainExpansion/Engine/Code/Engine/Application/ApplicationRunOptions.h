#pragma once

#include "Engine/Framework/BackendValidation.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Engine/Profiler/ProfilerBackend.h"
#include "Render/Backends/RenderBackend.h"

struct ApplicationRunOptions
{
	RenderBackendType backendType = RenderBackendType::dx12;
#if defined(_DEBUG)
	bool enableBackendDebugLayer = true;
#else
	bool enableBackendDebugLayer = false;
#endif
	BackendValidationInjectMode backendValidationInjectMode = BackendValidationInjectMode::none;
	ProfilerBackendType profilerBackendType = ProfilerBackendType::none;
	string profilerCaptureOutputFilePath = {};
	uint32 quitAfterFrameCount = 0;
};

ApplicationRunOptions parseApplicationRunOptions(WideStringPointer commandLine);
