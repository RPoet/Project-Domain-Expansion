#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/CommandList.h"

class CommandQueue
{
public:
	virtual ~CommandQueue() = default;

	virtual void execute(CommandList* commandList) = 0;
};
