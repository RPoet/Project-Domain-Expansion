#include "Engine/Profiler/ProfilerBackend.h"

#include "Engine/Profiler/Backends/NullProfilerBackend.h"
#include "Engine/Profiler/Backends/PerfettoProfilerBackend.h"

bool ProfilerBackend::create(const ProfilerBackendCreateOptions& options)
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

void ProfilerBackend::destroy()
{
	destroyBackendState();
	createOptions = {};
	createdState = false;
}

void ProfilerBackend::preUpdate()
{
}

void ProfilerBackend::postUpdate()
{
}

void ProfilerBackend::recordXMLDocumentLoad(const ProfilerXMLDocumentLoad& documentLoad)
{
	unused(documentLoad);
}

bool ProfilerBackend::isSupportedBackend(const ProfilerBackendType backendType)
{
	return backendType == ProfilerBackendType::none
		|| (backendType == ProfilerBackendType::perfetto && PerfettoProfilerBackend::isAvailable());
}

unique_pointer<ProfilerBackend> ProfilerBackend::createBackend(const ProfilerBackendType backendType)
{
	if (!isSupportedBackend(backendType))
	{
		return nullptr;
	}

	if (backendType == ProfilerBackendType::perfetto)
	{
		return unique_pointer<ProfilerBackend>(new PerfettoProfilerBackend());
	}

	return unique_pointer<ProfilerBackend>(new NullProfilerBackend());
}

const ProfilerBackendCreateOptions& ProfilerBackend::getCreateOptions() const
{
	return createOptions;
}

bool ProfilerBackend::isCreated() const
{
	return createdState;
}
