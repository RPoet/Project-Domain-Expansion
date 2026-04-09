#pragma once

#include "Engine/Profiler/ProfilerBackend.h"

ProfilerBackend* resolveActiveProfilerBackend();
const string& resolveEmptyProfilerDetail();

class ProfilerScope final
{
public:
	ProfilerScope(const char* category, const char* name)
	{
		ProfilerBackend* activeBackend = resolveActiveProfilerBackend();
		if (activeBackend == nullptr)
		{
			return;
		}

		backend = activeBackend;
		backend->beginEvent(category, name, resolveEmptyProfilerDetail());
	}

	ProfilerScope(const char* category, const char* name, const string& detail)
	{
		ProfilerBackend* activeBackend = resolveActiveProfilerBackend();
		if (activeBackend == nullptr)
		{
			return;
		}

		backend = activeBackend;
		backend->beginEvent(category, name, detail);
	}

	~ProfilerScope()
	{
		if (backend != nullptr)
		{
			backend->endEvent();
		}
	}

private:
	ProfilerBackend* backend = nullptr;
};

#define PROFILE_SCOPE_JOIN_IMPL(left, right) left##right
#define PROFILE_SCOPE_JOIN(left, right) PROFILE_SCOPE_JOIN_IMPL(left, right)
#define PROFILE_SCOPE(category, name) ProfilerScope PROFILE_SCOPE_JOIN(profilerScope_, __LINE__)(category, name)
#define PROFILE_SCOPE_DETAIL(category, name, detail) ProfilerScope PROFILE_SCOPE_JOIN(profilerScope_, __LINE__)(category, name, detail)
