#pragma once

class ProfilerScopeHookSuppressionGuard final
{
public:
	ProfilerScopeHookSuppressionGuard();
	~ProfilerScopeHookSuppressionGuard();

	ProfilerScopeHookSuppressionGuard(const ProfilerScopeHookSuppressionGuard&) = delete;
	ProfilerScopeHookSuppressionGuard& operator=(const ProfilerScopeHookSuppressionGuard&) = delete;
};

void beginVectorMallocProfileScope();
void endVectorMallocProfileScope();

void beginGlobalNewProfileScope();
void endGlobalNewProfileScope();
