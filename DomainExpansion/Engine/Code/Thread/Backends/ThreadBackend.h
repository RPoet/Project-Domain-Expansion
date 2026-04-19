#pragma once

#include "Thread/ThreadContext.h"

struct ThreadBackendCreateOptions
{
	ThreadBackendType backendType = getDefaultThreadBackendType();
	string name = {};
	ThreadRole role = ThreadRole::worker;
};

class ThreadBackend
{
public:
	virtual ~ThreadBackend() = default;
	ThreadBackend(const ThreadBackend&) = delete;
	ThreadBackend& operator=(const ThreadBackend&) = delete;
	ThreadBackend(ThreadBackend&&) = delete;
	ThreadBackend& operator=(ThreadBackend&&) = delete;

	bool create(const ThreadBackendCreateOptions& options);
	void destroy();
	virtual bool start(ThreadContext& threadContext, function<int32(ThreadContext&)>&& entry) = 0;
	virtual void requestStop() = 0;
	virtual bool isStopRequested() const = 0;
	virtual bool isRunning() const = 0;
	virtual bool hasExited() const = 0;
	virtual int32 getExitCode() const = 0;
	virtual void join() = 0;
	virtual uint64 getNativeThreadId() const = 0;
	virtual bool setThreadName(const string& threadName) = 0;

	static bool isSupportedBackend(ThreadBackendType backendType);
	static unique_pointer<ThreadBackend> createBackend(ThreadBackendType backendType);

protected:
	ThreadBackend() = default;
	const ThreadBackendCreateOptions& getCreateOptions() const;
	bool isCreated() const;
	virtual bool createBackendState() = 0;
	virtual void destroyBackendState() = 0;

private:
	ThreadBackendCreateOptions createOptions = {};
	bool createdState = false;
};
