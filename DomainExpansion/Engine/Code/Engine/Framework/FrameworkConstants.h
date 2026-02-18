#pragma once

#include "Engine/Platform/PlatformDefine.h"

inline constexpr uint32 invalidWorldIndex = 0xFFFFFFFFu;
inline constexpr uint32 invalidEntityIndex = 0xFFFFFFFFu;
inline constexpr uint32 invalidComponentIndex = 0xFFFFFFFFu;

enum class FrameworkRuntimeExitCode : int32
{
	success = 0,
	testFlowTickFailed = 1,
	backendFlowRuntimeFailure = 3,
	backendFlowSummaryFailure = 4,
	moduleInitializationFailure = 6,
};
