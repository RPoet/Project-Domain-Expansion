#include "Engine/Profiler/Backends/NullProfilerBackend.h"

bool NullProfilerBackend::beginCapture(const ProfilerCaptureOptions& captureOptions)
{
	unused(captureOptions);
	return false;
}

bool NullProfilerBackend::endCapture(ProfilerCaptureResult& outCaptureResult)
{
	outCaptureResult = {};
	return false;
}

bool NullProfilerBackend::isCaptureActive() const
{
	return false;
}

void NullProfilerBackend::beginEvent(const char* category, const char* name, const string& detail)
{
	unused(category);
	unused(name);
	unused(detail);
}

void NullProfilerBackend::endEvent()
{
}

bool NullProfilerBackend::createBackendState()
{
	return true;
}

void NullProfilerBackend::destroyBackendState()
{
}
