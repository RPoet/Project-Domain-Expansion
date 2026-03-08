#pragma once

#include "Engine/Module/Singleton.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Render/Backends/RenderBackend.h"

class RenderCommand final : public Singleton<RenderCommand>
{
public:
	using CommandFunction = function<void(string&&, RenderBackend&)>;
	using CommandPack = pair<string, CommandFunction>;

	struct RenderCommandFlushInput
	{
		bool clearOnly = false;
		bool validateAfterFlush = false;
		function<bool()> processBackendValidationFailFast = {};
		function<void()> onFlushed = {};
	};

	void enqueue(string&& name, CommandFunction&& commandFunction);
	void enqueue(const string& name, CommandFunction&& commandFunction);
	void flush();
	static void flushRenderCommandQueue(const RenderCommandFlushInput& flushInput);
	void clear();
	uint32 getPendingCommandCount() const;

private:
	friend class Singleton<RenderCommand>;

	RenderCommand() = default;
	~RenderCommand() = default;

	vector<CommandPack> commandQueue;
};
