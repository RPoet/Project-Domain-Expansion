#pragma once

#include "Engine/Platform/PlatformDefine.h"

class SyncObject
{
public:
	virtual ~SyncObject() = default;

	virtual void wait() = 0;
	virtual void wait(uint64 targetSyncValue) = 0;
	virtual uint64 signal() = 0;

	virtual uint64 getCompletedSyncValue() const = 0;
};
