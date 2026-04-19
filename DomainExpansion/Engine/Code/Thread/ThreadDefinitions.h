#pragma once

#include "Engine/Platform/PlatformDefine.h"

enum class ThreadBackendType : uint32
{
	window = 0,
	android = 1,
	apple = 2,
};

enum class ThreadRole : uint32
{
	unknown = 0,
	main = 1,
	worker = 2,
	io = 3,
	background = 4,
};

enum class ThreadState : uint32
{
	created = 0,
	starting = 1,
	running = 2,
	stopRequested = 3,
	exited = 4,
	joined = 5,
	failed = 6,
};

enum class JobQueueType : uint32
{
	worker = 0,
	io = 1,
	background = 2,
	mainThreadContinuation = 3,
};

enum class JobPriority : uint32
{
	high = 0,
	normal = 1,
	low = 2,
};

inline constexpr uint32 jobPriorityCount = 3;

inline constexpr uint32 getJobPriorityIndex(const JobPriority priority)
{
	const uint32 priorityIndex = static_cast<uint32>(priority);
	return priorityIndex < jobPriorityCount ? priorityIndex : 1;
}

inline constexpr ThreadBackendType getDefaultThreadBackendType()
{
#if defined(_WIN32)
	return ThreadBackendType::window;
#elif defined(__ANDROID__)
	return ThreadBackendType::android;
#elif defined(__APPLE__)
	return ThreadBackendType::apple;
#else
	return ThreadBackendType::window;
#endif
}
