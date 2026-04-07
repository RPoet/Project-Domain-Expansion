#pragma once

#include "Engine/Platform/PlatformDefine.h"

enum class ProfilerBackendType : uint32
{
	none = 0,
	perfetto = 1,
};

struct ProfilerBackendCreateOptions
{
	ProfilerBackendType backendType = ProfilerBackendType::none;
};

struct ProfilerCaptureOptions
{
	string captureName = {};
	string outputFilePath = {};
};

struct ProfilerCaptureResult
{
	string captureName = {};
	string outputFilePath = {};
	string xmlSummaryFilePath = {};
};

struct ProfilerXMLDocumentLoad
{
	string filePath = {};
	string parseResult = {};
	uint64 fileSizeBytes = 0;
	uint64 keyCount = 0;
	double readMilliseconds = 0.0;
	double parseMilliseconds = 0.0;
};

class ProfilerBackend
{
public:
	virtual ~ProfilerBackend() = default;
	ProfilerBackend(const ProfilerBackend&) = delete;
	ProfilerBackend& operator=(const ProfilerBackend&) = delete;
	ProfilerBackend(ProfilerBackend&&) = delete;
	ProfilerBackend& operator=(ProfilerBackend&&) = delete;

	bool create(const ProfilerBackendCreateOptions& options);
	void destroy();
	virtual void preUpdate();
	virtual void postUpdate();
	virtual bool beginCapture(const ProfilerCaptureOptions& captureOptions) = 0;
	virtual bool endCapture(ProfilerCaptureResult& outCaptureResult) = 0;
	virtual bool isCaptureActive() const = 0;
	virtual void beginEvent(const char* category, const char* name, const string& detail) = 0;
	virtual void endEvent() = 0;
	virtual void recordXMLDocumentLoad(const ProfilerXMLDocumentLoad& documentLoad);

	static bool isSupportedBackend(ProfilerBackendType backendType);
	static unique_pointer<ProfilerBackend> createBackend(ProfilerBackendType backendType);

protected:
	ProfilerBackend() = default;
	const ProfilerBackendCreateOptions& getCreateOptions() const;
	bool isCreated() const;
	virtual bool createBackendState() = 0;
	virtual void destroyBackendState() = 0;

private:
	ProfilerBackendCreateOptions createOptions = {};
	bool createdState = false;
};
