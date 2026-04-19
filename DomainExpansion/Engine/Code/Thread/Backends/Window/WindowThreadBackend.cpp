#include "Thread/Backends/Window/WindowThreadBackend.h"

unsigned long __stdcall WindowThreadBackend::runWindowThread(void* payloadPointer)
{
	unique_pointer<ThreadEntryPayload> payload(static_cast<ThreadEntryPayload*>(payloadPointer));
	assert(payload != nullptr && "[WindowThreadBackend][Assert] reason=thread_payload_missing");
	assert(payload->backend != nullptr && "[WindowThreadBackend][Assert] reason=thread_backend_missing");
	assert(payload->threadContext != nullptr && "[WindowThreadBackend][Assert] reason=thread_context_missing");
	assert(payload->entryFunction != nullptr && "[WindowThreadBackend][Assert] reason=thread_entry_missing");

	WindowThreadBackend& backend = *payload->backend;
	ThreadContext& threadContext = *payload->threadContext;
	setCurrentThreadContext(&threadContext);

	const uint64 threadId = static_cast<uint64>(GetCurrentThreadId());
	backend.nativeThreadId.store(threadId);
	threadContext.nativeThreadId = threadId;
	threadContext.state = ThreadState::running;
	backend.running.store(true);

	int32 resolvedExitCode = 0;
	try
	{
		resolvedExitCode = payload->entryFunction(threadContext);
	}
	catch (...)
	{
		threadContext.state = ThreadState::failed;
		resolvedExitCode = -1;
	}

	backend.exitCode.store(resolvedExitCode);
	backend.running.store(false);
	backend.exited.store(true);
	if (threadContext.state != ThreadState::failed)
	{
		threadContext.state = ThreadState::exited;
	}

	setCurrentThreadContext(nullptr);
	return static_cast<unsigned long>(resolvedExitCode);
}

bool WindowThreadBackend::start(ThreadContext& threadContext, function<int32(ThreadContext&)>&& entry)
{
	assert(isCreated() && "[WindowThreadBackend][Assert] reason=backend_not_created");
	assert(threadHandle == nullptr && "[WindowThreadBackend][Assert] reason=thread_already_started");
	assert(entry != nullptr && "[WindowThreadBackend][Assert] reason=entry_missing");

	activeThreadContext = &threadContext;
	stopRequested.store(false);
	running.store(false);
	exited.store(false);
	exitCode.store(0);
	nativeThreadId.store(0);

	threadContext.backendType = getCreateOptions().backendType;
	threadContext.role = getCreateOptions().role;
	threadContext.name = getCreateOptions().name;
	threadContext.nativeThreadId = 0;
	threadContext.stopRequested.store(false);
	threadContext.state = ThreadState::starting;

	unique_pointer<ThreadEntryPayload> payload(new ThreadEntryPayload{
		.backend = this,
		.threadContext = &threadContext,
		.entryFunction = moveValue(entry),
	});
	ThreadIdentifier threadId = 0;
	threadHandle = CreateThread(
		nullptr,
		0,
		&WindowThreadBackend::runWindowThread,
		payload.get(),
		0,
		&threadId);
	const bool createdThreadHandle = threadHandle != nullptr;
	assert(createdThreadHandle && "[WindowThreadBackend][Assert] reason=create_thread_failed");
	if (!createdThreadHandle)
	{
		return false;
	}

	payload.release();
	const uint64 nativeThreadIdentifier = static_cast<uint64>(threadId);
	nativeThreadId.store(nativeThreadIdentifier);
	threadContext.nativeThreadId = nativeThreadIdentifier;
	setThreadName(threadContext.name);
	return true;
}

void WindowThreadBackend::requestStop()
{
	stopRequested.store(true);
	if (activeThreadContext == nullptr)
	{
		return;
	}

	activeThreadContext->stopRequested.store(true);
	if (activeThreadContext->state == ThreadState::running)
	{
		activeThreadContext->state = ThreadState::stopRequested;
	}
}

bool WindowThreadBackend::isStopRequested() const
{
	return stopRequested.load();
}

bool WindowThreadBackend::isRunning() const
{
	return running.load();
}

bool WindowThreadBackend::hasExited() const
{
	return exited.load();
}

int32 WindowThreadBackend::getExitCode() const
{
	return exitCode.load();
}

void WindowThreadBackend::join()
{
	if (threadHandle == nullptr)
	{
		return;
	}

	const uint32 waitResult = WaitForSingleObject(threadHandle, INFINITE);
	assert(waitResult == WAIT_OBJECT_0 && "[WindowThreadBackend][Assert] reason=wait_thread_failed");
	const Bool closedThreadHandle = CloseHandle(threadHandle);
	assert(closedThreadHandle != boolFalse && "[WindowThreadBackend][Assert] reason=close_thread_handle_failed");
	threadHandle = nullptr;
	if (activeThreadContext != nullptr && activeThreadContext->state != ThreadState::failed)
	{
		activeThreadContext->state = ThreadState::joined;
	}
}

uint64 WindowThreadBackend::getNativeThreadId() const
{
	return nativeThreadId.load();
}

bool WindowThreadBackend::setThreadName(const string& threadName)
{
	if (threadName.empty() || threadHandle == nullptr)
	{
		return false;
	}

	const wstring wideThreadName(threadName.begin(), threadName.end());
	return SUCCEEDED(SetThreadDescription(threadHandle, wideThreadName.c_str()));
}

bool WindowThreadBackend::createBackendState()
{
	threadHandle = nullptr;
	activeThreadContext = nullptr;
	stopRequested.store(false);
	running.store(false);
	exited.store(false);
	exitCode.store(0);
	nativeThreadId.store(0);
	return true;
}

void WindowThreadBackend::destroyBackendState()
{
	assert(threadHandle == nullptr && "[WindowThreadBackend][Assert] reason=thread_destroy_without_join");
	threadHandle = nullptr;
	activeThreadContext = nullptr;
	stopRequested.store(false);
	running.store(false);
	exited.store(false);
	exitCode.store(0);
	nativeThreadId.store(0);
}
