#pragma once

#include "Engine/Platform/PlatformDefine.h"

class RenderTargetView
{
public:
	// TO DO : Replace this temporary view wrapper with a unified cross-platform view handle system.
	virtual ~RenderTargetView() = default;
};
