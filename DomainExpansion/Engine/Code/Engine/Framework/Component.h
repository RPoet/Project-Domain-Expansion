#pragma once

#include "Engine/Platform/PlatformDefine.h"

class Component
{
public:
	virtual ~Component() = default;
	virtual void tick(float deltaTimeSeconds)
	{
		unused(deltaTimeSeconds);
	}
};