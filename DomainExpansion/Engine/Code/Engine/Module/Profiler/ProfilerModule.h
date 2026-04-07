#pragma once

#include "Engine/Module/Module.h"
#include "Engine/Profiler/ProfilerBackend.h"

class ProfilerModule final : public StaticModule<ProfilerModule>
{
public:
	ProfilerModule()
		: StaticModule("ProfilerModule")
	{
	}

	bool init(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	bool createBackend(const ProfilerBackendCreateOptions& createOptions);
	void destroyBackend();
	bool beginCapture(const ProfilerCaptureOptions& captureOptions);
	bool endCapture(ProfilerCaptureResult& outCaptureResult);
	bool isCaptureActive() const;
	ProfilerBackend* getBackend();
	const ProfilerBackend* getBackend() const;
	bool isBackendCreated() const;
	void recordXMLDocumentLoad(const ProfilerXMLDocumentLoad& documentLoad);

private:
	unique_pointer<ProfilerBackend> profilerBackend = nullptr;
};
