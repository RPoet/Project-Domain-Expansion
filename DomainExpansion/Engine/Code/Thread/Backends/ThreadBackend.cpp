#include "Thread/Backends/ThreadBackend.h"

#include "Thread/Backends/Window/WindowThreadBackend.h"

bool ThreadBackend::create(const ThreadBackendCreateOptions& options)
{
	destroy();
	createOptions = options;
	if (!createBackendState())
	{
		destroy();
		return false;
	}

	createdState = true;
	return true;
}

void ThreadBackend::destroy()
{
	destroyBackendState();
	createOptions = {};
	createdState = false;
}

bool ThreadBackend::isSupportedBackend(const ThreadBackendType backendType)
{
	return backendType == ThreadBackendType::window;
}

unique_pointer<ThreadBackend> ThreadBackend::createBackend(const ThreadBackendType backendType)
{
	if (!isSupportedBackend(backendType))
	{
		return nullptr;
	}

	if (backendType == ThreadBackendType::window)
	{
		return unique_pointer<ThreadBackend>(new WindowThreadBackend());
	}

	return nullptr;
}

const ThreadBackendCreateOptions& ThreadBackend::getCreateOptions() const
{
	return createOptions;
}

bool ThreadBackend::isCreated() const
{
	return createdState;
}
