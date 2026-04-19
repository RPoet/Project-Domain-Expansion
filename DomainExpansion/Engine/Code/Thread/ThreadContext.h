#pragma once

#include "Thread/ThreadDefinitions.h"

struct ThreadContext
{
	uint32 logicalThreadIndex = 0;
	uint64 nativeThreadId = 0;
	ThreadBackendType backendType = getDefaultThreadBackendType();
	ThreadRole role = ThreadRole::unknown;
	ThreadState state = ThreadState::created;
	string name = {};
	atomic_bool stopRequested = false;
};

ThreadContext* getCurrentThreadContext();
const ThreadContext* getCurrentThreadContextConst();
void setCurrentThreadContext(ThreadContext* threadContext);
