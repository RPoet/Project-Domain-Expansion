#pragma once

#include "Engine/Profiler/ProfilerBackend.h"

class PerfettoProfilerBackend final : public ProfilerBackend
{
public:
	static bool isAvailable();
	bool beginCapture(const ProfilerCaptureOptions& captureOptions) override final;
	bool endCapture(ProfilerCaptureResult& outCaptureResult) override final;
	bool isCaptureActive() const override final;
	void beginEvent(const char* category, const char* name, const string& detail) override final;
	void endEvent() override final;
	void recordXMLDocumentLoad(const ProfilerXMLDocumentLoad& documentLoad) override final;

protected:
	bool createBackendState() override final;
	void destroyBackendState() override final;

private:
	struct ActiveTraceEvent
	{
		string category = {};
		string name = {};
		string detail = {};
		double beginTimeSeconds = 0.0;
	};

	struct CompletedTraceEvent
	{
		string category = {};
		string name = {};
		string detail = {};
		uint64 beginMicroseconds = 0;
		uint64 durationMicroseconds = 0;
	};

	bool writeTraceFile(const string& outputFilePath) const;
	bool writeXMLSummaryFile(const string& outputFilePath, string& outSummaryFilePath) const;
	void resetCaptureState();

	bool captureActive = false;
	double captureStartTimeSeconds = 0.0;
	ProfilerCaptureOptions activeCaptureOptions = {};
	vector<ActiveTraceEvent> activeEvents = {};
	vector<CompletedTraceEvent> completedEvents = {};
	vector<ProfilerXMLDocumentLoad> xmlDocumentLoads = {};
};
