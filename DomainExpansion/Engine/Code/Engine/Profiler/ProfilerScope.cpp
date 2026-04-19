#include "Engine/Profiler/ProfilerScopeHooks.h"
#include "Engine/Profiler/ProfilerScope.h"

#include "Engine/Module/Profiler/ProfilerModule.h"

static const string profilerEmptyDetail = {};

namespace
{
	thread_local uint32 vectorMallocProfileRecursionDepth = 0;
	thread_local bool vectorMallocProfileScopeActive = false;
	thread_local uint32 globalNewProfileRecursionDepth = 0;
	thread_local bool globalNewProfileScopeActive = false;
	thread_local uint32 profilerScopeHookSuppressionDepth = 0;
}

ProfilerScopeHookSuppressionGuard::ProfilerScopeHookSuppressionGuard()
{
	++profilerScopeHookSuppressionDepth;
}

ProfilerScopeHookSuppressionGuard::~ProfilerScopeHookSuppressionGuard()
{
	assert(profilerScopeHookSuppressionDepth > 0 && "[Profiler][Assert] reason=hook_suppression_depth_underflow");
	--profilerScopeHookSuppressionDepth;
}

const string& resolveEmptyProfilerDetail()
{
	return profilerEmptyDetail;
}

ProfilerBackend* resolveActiveProfilerBackend()
{
	return ProfilerModule::getActiveBackendFast();
}

void beginVectorMallocProfileScope()
{
	if (profilerScopeHookSuppressionDepth > 0)
	{
		return;
	}

	++vectorMallocProfileRecursionDepth;
	if (vectorMallocProfileRecursionDepth > 1)
	{
		return;
	}

	ProfilerBackend* activeBackend = resolveActiveProfilerBackend();
	if (activeBackend == nullptr)
	{
		vectorMallocProfileScopeActive = false;
		return;
	}

	ProfilerScopeHookSuppressionGuard hookSuppressionGuard = {};
	try
	{
		activeBackend->beginEvent("memory", "vector.malloc", resolveEmptyProfilerDetail());
	}
	catch (...)
	{
		throw;
	}
	vectorMallocProfileScopeActive = true;
}

void endVectorMallocProfileScope()
{
	if (vectorMallocProfileRecursionDepth == 0)
	{
		return;
	}

	--vectorMallocProfileRecursionDepth;
	if (vectorMallocProfileRecursionDepth > 0 || !vectorMallocProfileScopeActive)
	{
		return;
	}

	ProfilerBackend* activeBackend = resolveActiveProfilerBackend();
	if (activeBackend != nullptr)
	{
		ProfilerScopeHookSuppressionGuard hookSuppressionGuard = {};
		try
		{
			activeBackend->endEvent();
		}
		catch (...)
		{
			throw;
		}
	}

	vectorMallocProfileScopeActive = false;
}

void beginGlobalNewProfileScope()
{
	if (profilerScopeHookSuppressionDepth > 0)
	{
		return;
	}

	++globalNewProfileRecursionDepth;
	if (globalNewProfileRecursionDepth > 1)
	{
		return;
	}

	ProfilerBackend* activeBackend = resolveActiveProfilerBackend();
	if (activeBackend == nullptr)
	{
		globalNewProfileScopeActive = false;
		return;
	}

	ProfilerScopeHookSuppressionGuard hookSuppressionGuard = {};
	try
	{
		activeBackend->beginEvent("memory", "global_new", resolveEmptyProfilerDetail());
	}
	catch (...)
	{
		throw;
	}
	globalNewProfileScopeActive = true;
}

void endGlobalNewProfileScope()
{
	if (globalNewProfileRecursionDepth == 0)
	{
		return;
	}

	--globalNewProfileRecursionDepth;
	if (globalNewProfileRecursionDepth > 0 || !globalNewProfileScopeActive)
	{
		return;
	}

	ProfilerBackend* activeBackend = resolveActiveProfilerBackend();
	if (activeBackend != nullptr)
	{
		ProfilerScopeHookSuppressionGuard hookSuppressionGuard = {};
		try
		{
			activeBackend->endEvent();
		}
		catch (...)
		{
			throw;
		}
	}

	globalNewProfileScopeActive = false;
}
