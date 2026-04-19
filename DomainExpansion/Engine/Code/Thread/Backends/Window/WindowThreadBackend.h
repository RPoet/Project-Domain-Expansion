#pragma once

#include "Thread/Backends/ThreadBackend.h"

class WindowThreadBackend final : public ThreadBackend
{
public:
	bool start(ThreadContext& threadContext, function<int32(ThreadContext&)>&& entry) override final;
	void requestStop() override final;
	bool isStopRequested() const override final;
	bool isRunning() const override final;
	bool hasExited() const override final;
	int32 getExitCode() const override final;
	void join() override final;
	uint64 getNativeThreadId() const override final;
	bool setThreadName(const string& threadName) override final;

protected:
	bool createBackendState() override final;
	void destroyBackendState() override final;

private:
	struct ThreadEntryPayload
	{
		WindowThreadBackend* backend = nullptr;
		ThreadContext* threadContext = nullptr;
		function<int32(ThreadContext&)> entryFunction = {};
	};

	static unsigned long __stdcall runWindowThread(void* payloadPointer);

	HandleThread threadHandle = nullptr;
	ThreadContext* activeThreadContext = nullptr;
	atomic_bool stopRequested = false;
	atomic_bool running = false;
	atomic_bool exited = false;
	atomic<int32> exitCode = 0;
	atomic<uint64> nativeThreadId = 0;
};
