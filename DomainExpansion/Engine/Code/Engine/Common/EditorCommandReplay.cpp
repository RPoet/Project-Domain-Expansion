#include "Engine/Common/EditorCommandReplay.h"
#include "Engine/Common/StringSlice.h"

#include "Engine/Module/DiskLoader/DiskLoaderModule.h"

static string trimReplayCommandLine(const string& lineText)
{
	size_t beginIndex = 0;
	while (beginIndex < lineText.length() && (lineText[beginIndex] == ' ' || lineText[beginIndex] == '\t'))
	{
		++beginIndex;
	}

	size_t endIndex = lineText.length();
	while (endIndex > beginIndex
		&& (lineText[endIndex - 1] == ' ' || lineText[endIndex - 1] == '\t' || lineText[endIndex - 1] == '\r'))
	{
		--endIndex;
	}

	return sliceString(lineText, beginIndex, endIndex - beginIndex);
}

bool EditorCommandReplay::resolveDefaultLogFilePath(string& outLogFilePath)
{
	outLogFilePath.clear();
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[EditorCommandReplay][Assert] reason=disk_loader_module_missing");

	string solutionRootPath = {};
	if (!diskLoaderModule->TEMP_resolveSolutionRootPath(solutionRootPath))
	{
		return false;
	}

	outLogFilePath = (filesystem_path(solutionRootPath) / "editor_replay.log").lexically_normal().string();
	return true;
}

string EditorCommandReplay::buildCommandText(const string& commandName, const vector<string>& arguments)
{
	assert(!commandName.empty() && "[EditorCommandReplay][Assert] reason=command_name_missing");

	string commandText = commandName;
	for (uint32 argumentIndex = 0; argumentIndex < static_cast<uint32>(arguments.size()); ++argumentIndex)
	{
		commandText += " \"";
		commandText += escapeCommandArgument(arguments[argumentIndex]);
		commandText += "\"";
	}

	return commandText;
}

bool EditorCommandReplay::appendCommand(const string& commandName, const vector<string>& arguments)
{
	return appendCommandText(buildCommandText(commandName, arguments));
}

bool EditorCommandReplay::appendCommandText(const string& commandText)
{
	string logFilePath = {};
	if (!resolveDefaultLogFilePath(logFilePath))
	{
		return false;
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[EditorCommandReplay][Assert] reason=disk_loader_module_missing");
	if (!diskLoaderModule->ensureParentDirectory(logFilePath))
	{
		return false;
	}

	OutputFileStream fileStream = {};
	fileStream.open(logFilePath, output_file_stream::out | output_file_stream::app);
	if (!fileStream.is_open() || !fileStream.good())
	{
		return false;
	}

	fileStream << commandText << '\n';
	return fileStream.good();
}

bool EditorCommandReplay::clearLog(const string& requestedLogFilePath)
{
	string logFilePath = {};
	if (!resolveLogFilePath(requestedLogFilePath, logFilePath))
	{
		return false;
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[EditorCommandReplay][Assert] reason=disk_loader_module_missing");
	if (!diskLoaderModule->ensureParentDirectory(logFilePath))
	{
		return false;
	}

	OutputFileStream fileStream = {};
	fileStream.open(logFilePath, output_file_stream::out | output_file_stream::trunc);
	return fileStream.is_open() && fileStream.good();
}

bool EditorCommandReplay::loadCommands(
	const string& requestedLogFilePath,
	vector<string>& outCommandTexts,
	string* outResolvedLogFilePath)
{
	outCommandTexts.clear();
	if (outResolvedLogFilePath != nullptr)
	{
		outResolvedLogFilePath->clear();
	}

	string logFilePath = {};
	if (!resolveLogFilePath(requestedLogFilePath, logFilePath))
	{
		return false;
	}

	if (outResolvedLogFilePath != nullptr)
	{
		*outResolvedLogFilePath = logFilePath;
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[EditorCommandReplay][Assert] reason=disk_loader_module_missing");

	InputFileStream fileStream = {};
	if (!diskLoaderModule->openInputFileStream(logFilePath, fileStream, false))
	{
		return false;
	}

	string lineText = {};
	while (std::getline(fileStream, lineText))
	{
		if (shouldIgnoreCommandLine(lineText))
		{
			continue;
		}

		outCommandTexts.push_back(lineText);
	}

	return fileStream.good() || fileStream.eof();
}

bool EditorCommandReplay::resolveLogFilePath(const string& requestedLogFilePath, string& outResolvedLogFilePath)
{
	outResolvedLogFilePath.clear();
	if (requestedLogFilePath.empty())
	{
		return resolveDefaultLogFilePath(outResolvedLogFilePath);
	}

	const filesystem_path requestedPath(requestedLogFilePath);
	if (requestedPath.is_absolute())
	{
		outResolvedLogFilePath = requestedPath.lexically_normal().string();
		return true;
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[EditorCommandReplay][Assert] reason=disk_loader_module_missing");

	string solutionRootPath = {};
	if (!diskLoaderModule->TEMP_resolveSolutionRootPath(solutionRootPath))
	{
		return false;
	}

	outResolvedLogFilePath = (filesystem_path(solutionRootPath) / requestedPath).lexically_normal().string();
	return true;
}

string EditorCommandReplay::escapeCommandArgument(const string& argument)
{
	string escapedArgument = {};
	escapedArgument.reserve(argument.length());
	for (size_t characterIndex = 0; characterIndex < argument.length(); ++characterIndex)
	{
		const char character = argument[characterIndex];
		switch (character)
		{
		case '\\':
			escapedArgument += "\\\\";
			break;
		case '"':
			escapedArgument += "\\\"";
			break;
		case '\n':
			escapedArgument += "\\n";
			break;
		case '\r':
			escapedArgument += "\\r";
			break;
		case '\t':
			escapedArgument += "\\t";
			break;
		default:
			escapedArgument.push_back(character);
			break;
		}
	}

	return escapedArgument;
}

bool EditorCommandReplay::shouldIgnoreCommandLine(const string& lineText)
{
	const string trimmedLineText = trimReplayCommandLine(lineText);
	return trimmedLineText.empty() || trimmedLineText[0] == '#';
}
