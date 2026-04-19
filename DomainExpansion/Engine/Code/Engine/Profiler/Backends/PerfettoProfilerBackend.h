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
	struct ThreadCaptureState;

	struct ActiveTraceEvent
	{
		const char* category = nullptr;
		const char* name = nullptr;
		string detail = {};
		double beginTimeSeconds = 0.0;
	};

	struct CompletedTraceEvent
	{
		const char* category = nullptr;
		const char* name = nullptr;
		string detail = {};
		uint64 threadId = 0;
		uint64 beginMicroseconds = 0;
		uint64 durationMicroseconds = 0;
	};

	bool writeTraceFile(const string& outputFilePath) const;
	bool writeXMLSummaryFile(const string& outputFilePath, string& outSummaryFilePath) const;
	ThreadCaptureState& acquireThreadCaptureState();
	void registerThreadCaptureState(ThreadCaptureState& threadCaptureState);
	void waitForThreadCaptureStatesIdle() const;
	void mergeThreadCaptureStates(const double captureEndTimeSeconds);
	void resetCaptureState();

	atomic_bool captureActive = false;
	atomic<double> captureStartTimeSeconds = 0.0;
	atomic<uint64> captureGeneration = 0;
	atomic<ThreadCaptureState*> captureThreadStateHead = nullptr;
	atomic<uint32> activeOperationCount = 0;
	ProfilerCaptureOptions activeCaptureOptions = {};
	vector<CompletedTraceEvent> completedEvents = {};
	unordered_map<uint64, string> threadNameById = {};
	vector<ProfilerXMLDocumentLoad> xmlDocumentLoads = {};
};
