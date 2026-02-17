#pragma once

#include "Engine/Platform/PlatformDefine.h"

class SyncObject
{
public:
	virtual ~SyncObject() = default;

	virtual void wait() = 0;
	virtual void signal() = 0;
};
