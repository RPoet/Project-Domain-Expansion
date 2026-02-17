#pragma once

#include "Engine/Platform/PlatformDefine.h"

enum class ResourceState : uint32
{
	unknown = 0,
	present = 1,
	renderTarget = 2,
};

