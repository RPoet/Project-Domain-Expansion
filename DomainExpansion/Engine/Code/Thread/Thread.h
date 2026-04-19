#pragma once

#include "Thread/Backends/ThreadBackend.h"

struct ThreadCreateOptions
{
	string name = {};
	ThreadRole role = ThreadRole::worker;
	ThreadBackendType backendType = getDefaultThreadBackendType();
	bool autoJoinOnDestroy = true;
};

class Thread final
{
public:
	Thread() = default;
	~Thread();
	Thread(const Thread&) = delete;
	Thread& operator=(const Thread&) = delete;
	Thread(Thread&&) = delete;
	Thread& operator=(Thread&&) = delete;

	bool create(const ThreadCreateOptions& createOptions);
	void destroy();
	bool start(function<int32(ThreadContext&)>&& entry);
	void requestStop();
	bool isStopRequested() const;
	bool isRunning() const;
	bool hasExited() const;
	int32 getExitCode() const;
	void join();
	ThreadContext* getContext();
	const ThreadContext* getContext() const;

private:
	ThreadCreateOptions createOptions = {};
	ThreadContext context = {};
	unique_pointer<ThreadBackend> backend = nullptr;
};
