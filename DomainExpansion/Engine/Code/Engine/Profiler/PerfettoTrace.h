#pragma once

#include "Engine/Platform/PlatformDefine.h"

struct PerfettoCaptureOptions
{
	string outputFilePath = {};
	uint32 stopAfterFrameCount = 1;
};

struct PerfettoStartupCaptureRequest
{
	bool enabled = false;
	uint32 stopAfterFrameCount = 1;
	string outputFilePath = {};
};

class PerfettoCapture final
{
public:
	PerfettoCapture() = default;
	~PerfettoCapture();

	PerfettoCapture(const PerfettoCapture&) = delete;
	PerfettoCapture& operator=(const PerfettoCapture&) = delete;
	PerfettoCapture(PerfettoCapture&&) = delete;
	PerfettoCapture& operator=(PerfettoCapture&&) = delete;

	static void initialize();

	void begin(const PerfettoCaptureOptions& captureOptions);
	void beginStartupCapture(const PerfettoStartupCaptureRequest& startupCaptureRequest);
	void end();
	void endIfActive();
	void onFramePresented();
	void onFramePresentedAndStopIfNeeded();
	bool shouldStopAfterPresentedFrame() const;
	bool isActive() const;
	const string& getOutputFilePath() const;

private:
	static string buildDefaultStartupOutputFilePath();

	struct State;
	State* state = nullptr;
	string outputFilePath = {};
	uint32 remainingFrameCount = 0;
};
