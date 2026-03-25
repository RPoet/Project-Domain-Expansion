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
	if (!commandQueue.empty())
	{
		shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
		RenderBackend* renderBackend = renderBackendModule != nullptr
			? renderBackendModule->getBackend()
			: nullptr;
		const bool validRenderBackend = renderBackendModule != nullptr && renderBackend != nullptr;
		assert(validRenderBackend && "[RenderCommand][Assert] reason=render_backend_missing");

		for (uint32 commandIndex = 0; commandIndex < static_cast<uint32>(commandQueue.size()); ++commandIndex)
		{
			CommandPack& commandPack = commandQueue[commandIndex];
			commandPack.second(moveValue(commandPack.first), *renderBackend);
		}
	}

	clear();
}

void RenderCommand::flushRenderCommandQueue(const RenderCommandFlushInput& flushInput)
{
	if (flushInput.clearOnly)
	{
		RenderCommand::get().clear();
		return;
	}

	RenderCommand::get().flush();
	if (flushInput.validateAfterFlush
		&& flushInput.processBackendValidationFailFast
		&& flushInput.processBackendValidationFailFast())
	{
		return;
	}

	if (flushInput.onFlushed)
	{
		flushInput.onFlushed();
	}
}

void RenderCommand::clear()
{
	commandQueue.clear();
}

uint32 RenderCommand::getPendingCommandCount() const
{
	return static_cast<uint32>(commandQueue.size());
}
