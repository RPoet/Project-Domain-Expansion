#pragma once

#include "Engine/Profiler/ProfilerBackend.h"

class NullProfilerBackend final : public ProfilerBackend
{
public:
	bool beginCapture(const ProfilerCaptureOptions& captureOptions) override final;
	bool endCapture(ProfilerCaptureResult& outCaptureResult) override final;
	bool isCaptureActive() const override final;
	void beginEvent(const char* category, const char* name, const string& detail) override final;
	void endEvent() override final;

protected:
	bool createBackendState() override final;
	void destroyBackendState() override final;
};
