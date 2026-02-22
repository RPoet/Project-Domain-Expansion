#pragma once

#include "Engine/Platform/PlatformDefine.h"

class SyncObject
{
public:
	virtual ~SyncObject() = default;

	virtual void wait() = 0;
	virtual void signal() = 0;

	// TO DO : Vk compatibility must be considered, the name of it has a bit different meaning.
	virtual uint64 getCompletedFenceValue() const = 0;
};
