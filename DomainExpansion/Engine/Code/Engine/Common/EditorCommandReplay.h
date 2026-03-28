#pragma once

#include "Engine/Platform/PlatformDefine.h"

class EditorCommandReplay final
{
public:
	static bool resolveDefaultLogFilePath(string& outLogFilePath);
	static string buildCommandText(const string& commandName, const vector<string>& arguments);
	static bool appendCommand(const string& commandName, const vector<string>& arguments);
	static bool appendCommandText(const string& commandText);
	static bool clearLog(const string& requestedLogFilePath = "");
	static bool loadCommands(const string& requestedLogFilePath, vector<string>& outCommandTexts, string* outResolvedLogFilePath = nullptr);

private:
	static bool resolveLogFilePath(const string& requestedLogFilePath, string& outResolvedLogFilePath);
	static string escapeCommandArgument(const string& argument);
	static bool shouldIgnoreCommandLine(const string& lineText);
};
