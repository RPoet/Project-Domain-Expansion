#pragma once

#include "Engine/Platform/PlatformDefine.h"

class BaseBridge
{
public:
	virtual ~BaseBridge() = default;
	virtual void processFrame() = 0;
};
