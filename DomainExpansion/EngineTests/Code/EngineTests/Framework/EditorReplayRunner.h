#pragma once

#include "Engine/Platform/PlatformDefine.h"

class EditorReplayRunner final
{
public:
	enum class ExecutionCode : int32
	{
		succeeded = 0,
		logReadFailed = -1,
		commandFailed = -2,
	};

	bool load(const string& requestedLogFilePath = "");
	bool step();
	bool replayImmediate();
	void clear();

	bool hasPendingCommands() const;
	bool isCompleted() const;
	bool hasFailed() const;
	uint32 getCommandCount() const;
	uint32 getNextCommandIndex() const;
	int32 getLastExecutionCode() const;
	const string& getResolvedLogFilePath() const;
	const string& getFailedCommandText() const;

private:
	vector<string> commandTexts = {};
	string resolvedLogFilePath = {};
	string failedCommandText = {};
	uint32 nextCommandIndex = 0;
	int32 lastExecutionCode = static_cast<int32>(ExecutionCode::succeeded);
	bool completed = false;
	bool failed = false;
};
