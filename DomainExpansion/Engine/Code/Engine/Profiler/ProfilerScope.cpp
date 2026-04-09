#include "Engine/Profiler/ProfilerScope.h"

#include "Engine/Module/Profiler/ProfilerModule.h"

static const string profilerEmptyDetail = {};

const string& resolveEmptyProfilerDetail()
{
	return profilerEmptyDetail;
}

ProfilerBackend* resolveActiveProfilerBackend()
{
	return ProfilerModule::getActiveBackendFast();
}
