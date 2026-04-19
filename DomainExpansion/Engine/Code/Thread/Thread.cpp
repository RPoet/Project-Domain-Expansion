#include "Thread/Thread.h"

static thread_local ThreadContext* currentThreadContext = nullptr;

ThreadContext* getCurrentThreadContext()
{
	return currentThreadContext;
}

const ThreadContext* getCurrentThreadContextConst()
{
	return currentThreadContext;
}

void setCurrentThreadContext(ThreadContext* threadContext)
{
	currentThreadContext = threadContext;
}

Thread::~Thread()
{
	destroy();
}

bool Thread::create(const ThreadCreateOptions& inCreateOptions)
{
	destroy();
	createOptions = inCreateOptions;
	backend = ThreadBackend::createBackend(createOptions.backendType);
	assert(backend != nullptr && "[Thread][Assert] reason=backend_create_failed");

	const bool createdBackend = backend->create({
		.backendType = createOptions.backendType,
		.name = createOptions.name,
		.role = createOptions.role,
	});
	assert(createdBackend && "[Thread][Assert] reason=backend_init_failed");
	if (!createdBackend)
	{
		backend.reset();
		return false;
	}

	context.backendType = createOptions.backendType;
	context.role = createOptions.role;
	context.state = ThreadState::created;
	context.name = createOptions.name;
	context.logicalThreadIndex = 0;
	context.nativeThreadId = 0;
	context.stopRequested.store(false);
	return true;
}

void Thread::destroy()
{
	if (backend == nullptr)
	{
		return;
	}

	if ((backend->isRunning() || backend->hasExited()) && createOptions.autoJoinOnDestroy)
	{
		requestStop();
		join();
	}

	assert(!backend->isRunning() && "[Thread][Assert] reason=destroy_running_thread");
	backend->destroy();
	backend.reset();
	context.logicalThreadIndex = 0;
	context.nativeThreadId = 0;
	context.backendType = getDefaultThreadBackendType();
	context.role = ThreadRole::unknown;
	context.state = ThreadState::created;
	context.name.clear();
	context.stopRequested.store(false);
	createOptions = {};
}

bool Thread::start(function<int32(ThreadContext&)>&& entry)
{
	assert(backend != nullptr && "[Thread][Assert] reason=start_without_backend");
	return backend->start(context, moveValue(entry));
}

void Thread::requestStop()
{
	if (backend != nullptr)
	{
		backend->requestStop();
	}
}

bool Thread::isStopRequested() const
{
	return backend == nullptr ? false : backend->isStopRequested();
}

bool Thread::isRunning() const
{
	return backend == nullptr ? false : backend->isRunning();
}

bool Thread::hasExited() const
{
	return backend == nullptr ? false : backend->hasExited();
}

int32 Thread::getExitCode() const
{
	return backend == nullptr ? 0 : backend->getExitCode();
}

void Thread::join()
{
	if (backend != nullptr)
	{
		backend->join();
	}
}

ThreadContext* Thread::getContext()
{
	return &context;
}

const ThreadContext* Thread::getContext() const
{
	return &context;
}
