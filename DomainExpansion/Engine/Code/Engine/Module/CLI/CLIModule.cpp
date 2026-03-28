#include "Engine/Module/CLI/CLIModule.h"

bool CLIModule::init(Framework& framework)
{
	unused(framework);
	clearLastCommand();
	initialized = true;
	return true;
}

void CLIModule::preUpdate()
{
}

void CLIModule::postUpdate()
{
}

void CLIModule::shutdown()
{
	clearLastCommand();
	initialized = false;
}

bool CLIModule::execute(const string& commandText)
{
	shared_pointer<CLIModule> cliModule = CLIModule::get();
	const bool validModule = cliModule != nullptr && cliModule->initialized;
	assert(validModule && "[CLIModule][Assert] reason=module_missing_or_not_initialized");
	return cliModule->executeInternal(commandText);
}

bool CLIModule::registerCommand(const string& commandName, const CommandHandler& commandHandler)
{
	shared_pointer<CLIModule> cliModule = CLIModule::get();
	const bool validModule = cliModule != nullptr;
	assert(validModule && "[CLIModule][Assert] reason=module_missing");
	return cliModule->registerCommandInternal(commandName, commandHandler);
}

const CLIModule::Command& CLIModule::getLastCommand() const
{
	return lastCommand;
}

const string& CLIModule::getLastCommandText() const
{
	return lastCommandText;
}

int32 CLIModule::getLastExecutionCode() const
{
	return lastExecutionCode;
}

bool CLIModule::registerCommandInternal(const string& commandName, const CommandHandler& commandHandler)
{
	const bool validRegistration = !commandName.empty()
		&& static_cast<bool>(commandHandler)
		&& registeredCommandByName.find(commandName) == registeredCommandByName.end();
	assert(validRegistration && "[CLIModule][Assert] reason=invalid_or_duplicate_command_registration");
	RegisteredCommand registeredCommand = {};
	registeredCommand.handler = commandHandler;
	registeredCommandByName[commandName] = moveValue(registeredCommand);
	return true;
}

bool CLIModule::parseAndDispatchCommandText(
	const string& commandText,
	Command& outCommand,
	int32& outExecutionCode) const
{
	outCommand = {};
	outExecutionCode = static_cast<int32>(ExecutionCode::succeeded);
	if (!parseCommandText(commandText, outCommand))
	{
		outExecutionCode = static_cast<int32>(ExecutionCode::parseFailed);
		return false;
	}

	const auto registeredCommandIterator = registeredCommandByName.find(outCommand.name);
	if (registeredCommandIterator == registeredCommandByName.end())
	{
		outExecutionCode = static_cast<int32>(ExecutionCode::commandNotRegistered);
		return false;
	}

	outExecutionCode = registeredCommandIterator->second.handler(outCommand.arguments);
	return true;
}

bool CLIModule::executeInternal(const string& commandText)
{
	clearLastCommand();
	lastCommandText = commandText;

	Command parsedCommand = {};
	lastExecutionCode = static_cast<int32>(ExecutionCode::succeeded);
	const bool executedCommand = parseAndDispatchCommandText(commandText, parsedCommand, lastExecutionCode);
	lastCommand = moveValue(parsedCommand);
	return executedCommand;
}

bool CLIModule::parseCommandText(const string& commandText, Command& outCommand) const
{
	outCommand = {};

	vector<string> tokens = {};
	string activeToken = {};
	activeToken.reserve(commandText.length());
	bool tokenStarted = false;
	bool quotedText = false;
	const auto flushToken = [&tokens, &activeToken, &tokenStarted]()
	{
		if (!tokenStarted)
		{
			return;
		}

		tokens.push_back(activeToken);
		activeToken.clear();
		tokenStarted = false;
	};

	for (size_t characterIndex = 0; characterIndex < commandText.length(); ++characterIndex)
	{
		const char character = commandText[characterIndex];
		if (quotedText && character == '\\')
		{
			if (characterIndex + 1 >= commandText.length())
			{
				return false;
			}

			const char escapedCharacter = commandText[++characterIndex];
			switch (escapedCharacter)
			{
			case 'n':
				activeToken.push_back('\n');
				break;
			case 'r':
				activeToken.push_back('\r');
				break;
			case 't':
				activeToken.push_back('\t');
				break;
			case '"':
				activeToken.push_back('"');
				break;
			case '\\':
				activeToken.push_back('\\');
				break;
			default:
				activeToken.push_back(escapedCharacter);
				break;
			}

			tokenStarted = true;
			continue;
		}

		if (character == '"')
		{
			quotedText = !quotedText;
			tokenStarted = true;
			continue;
		}

		const bool whiteSpaceCharacter = character == ' '
			|| character == '\t'
			|| character == '\n'
			|| character == '\r';
		if (!quotedText && whiteSpaceCharacter)
		{
			flushToken();
			continue;
		}

		activeToken.push_back(character);
		tokenStarted = true;
	}

	if (quotedText)
	{
		return false;
	}

	flushToken();
	if (tokens.empty())
	{
		return false;
	}

	outCommand.name = tokens[0];
	for (uint32 tokenIndex = 1; tokenIndex < static_cast<uint32>(tokens.size()); ++tokenIndex)
	{
		outCommand.arguments.push_back(tokens[tokenIndex]);
	}

	return !outCommand.name.empty();
}

void CLIModule::clearLastCommand()
{
	lastCommand = {};
	lastCommandText.clear();
	lastExecutionCode = static_cast<int32>(ExecutionCode::succeeded);
}
