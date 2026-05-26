#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/Backends/CommandList.h"

class CommandQueue
{
public:
	virtual ~CommandQueue() = default;

	virtual void enqueue(CommandList* commandList) = 0;
	virtual void executeQueued() = 0;
	virtual void clearQueued() = 0;
	virtual void execute(CommandList* commandList)
	{
		if (commandList == nullptr)
		{
			return;
		}

		enqueue(commandList);
		executeQueued();
		clearQueued();
	}
};
