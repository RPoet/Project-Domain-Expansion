#include "EngineTests/Framework/EditorReplayRunner.h"

#include "Engine/Common/EditorCommandReplay.h"
#include "Engine/Module/CLI/CLIModule.h"

bool EditorReplayRunner::load(const string& requestedLogFilePath)
{
	clear();
	if (!EditorCommandReplay::loadCommands(requestedLogFilePath, commandTexts, &resolvedLogFilePath))
	{
		lastExecutionCode = static_cast<int32>(ExecutionCode::logReadFailed);
		failed = true;
		completed = true;
		return false;
	}

	completed = commandTexts.empty();
	output << "[EditorReplayTest] action=load"
		   << " path=" << resolvedLogFilePath
		   << " count=" << commandTexts.size()
		   << lineBreak;
	return true;
}

bool EditorReplayRunner::step()
{
	if (failed || completed || nextCommandIndex >= static_cast<uint32>(commandTexts.size()))
	{
		return false;
	}

	const string& commandText = commandTexts[nextCommandIndex];
	const bool commandExecuted = CLIModule::execute(commandText);
	lastExecutionCode = CLIModule::get()->getLastExecutionCode();
	if (!commandExecuted || lastExecutionCode != static_cast<int32>(CLIModule::ExecutionCode::succeeded))
	{
		failed = true;
		completed = true;
		failedCommandText = commandText;
		error << "[EditorReplayTest] action=failed"
			  << " index=" << (nextCommandIndex + 1)
			  << " total=" << commandTexts.size()
			  << " executionCode=" << lastExecutionCode
			  << " command=" << failedCommandText
			  << lineBreak;
		return false;
	}

	output << "[EditorReplayTest] action=step"
		   << " index=" << (nextCommandIndex + 1)
		   << " total=" << commandTexts.size()
		   << " command=" << commandText
		   << lineBreak;
	++nextCommandIndex;
	completed = nextCommandIndex >= static_cast<uint32>(commandTexts.size());
	if (completed)
	{
		output << "[EditorReplayTest] action=complete"
			   << " path=" << resolvedLogFilePath
			   << " count=" << commandTexts.size()
			   << lineBreak;
	}

	return true;
}

bool EditorReplayRunner::replayImmediate()
{
	while (hasPendingCommands())
	{
		if (!step())
		{
			return false;
		}
	}

	return !failed;
}

void EditorReplayRunner::clear()
{
	commandTexts.clear();
	resolvedLogFilePath.clear();
	failedCommandText.clear();
	nextCommandIndex = 0;
	lastExecutionCode = static_cast<int32>(ExecutionCode::succeeded);
	completed = false;
	failed = false;
}

bool EditorReplayRunner::hasPendingCommands() const
{
	return !failed && !completed && nextCommandIndex < static_cast<uint32>(commandTexts.size());
}

bool EditorReplayRunner::isCompleted() const
{
	return completed;
}

bool EditorReplayRunner::hasFailed() const
{
	return failed;
}

uint32 EditorReplayRunner::getCommandCount() const
{
	return static_cast<uint32>(commandTexts.size());
}

uint32 EditorReplayRunner::getNextCommandIndex() const
{
	return nextCommandIndex;
}

int32 EditorReplayRunner::getLastExecutionCode() const
{
	return lastExecutionCode;
}

const string& EditorReplayRunner::getResolvedLogFilePath() const
{
	return resolvedLogFilePath;
}

const string& EditorReplayRunner::getFailedCommandText() const
{
	return failedCommandText;
}
