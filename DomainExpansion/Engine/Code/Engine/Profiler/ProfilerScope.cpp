#include "Engine/Profiler/ProfilerScope.h"

#include "Engine/Module/Profiler/ProfilerModule.h"

ProfilerBackend* resolveActiveProfilerBackend()
{
	shared_pointer<ProfilerModule> profilerModule = ProfilerModule::get();
	if (profilerModule == nullptr || !profilerModule->isCaptureActive())
	{
		return nullptr;
	}

	return profilerModule->getBackend();
}
