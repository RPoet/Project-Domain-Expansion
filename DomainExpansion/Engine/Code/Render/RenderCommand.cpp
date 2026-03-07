#include "Render/RenderCommand.h"

#include "Engine/Module/Render/RenderBackendModule.h"

void RenderCommand::enqueue(string&& name, CommandFunction&& commandFunction)
{
	commandQueue.emplace_back(moveValue(name), moveValue(commandFunction));
}

void RenderCommand::enqueue(const string& name, CommandFunction&& commandFunction)
{
	commandQueue.emplace_back(name, moveValue(commandFunction));
}

void RenderCommand::flush()
{
	bool runCommand = !commandQueue.empty();
	RenderBackend* renderBackend = nullptr;

	if (runCommand)
	{
		shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
		if (renderBackendModule == nullptr)
		{
			error << "[RenderCommand][Error] backend_module_missing" << lineBreak;
			runCommand = false;
		}
		else
		{
			renderBackend = renderBackendModule->getBackend();
			if (renderBackend == nullptr)
			{
				error << "[RenderCommand][Error] backend_missing" << lineBreak;
				runCommand = false;
			}
		}
	}

	if (runCommand)
	{
		for (uint32 commandIndex = 0; commandIndex < static_cast<uint32>(commandQueue.size()); ++commandIndex)
		{
			CommandPack& commandPack = commandQueue[commandIndex];
			commandPack.second(moveValue(commandPack.first), *renderBackend);
		}
	}

	clear();
}

void RenderCommand::clear()
{
	commandQueue.clear();
}

uint32 RenderCommand::getPendingCommandCount() const
{
	return static_cast<uint32>(commandQueue.size());
}
