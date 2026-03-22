#include "Engine/Module/CLI/CLIModule.h"

#include "Engine/Framework/Framework.h"

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

bool CLIModule::executeInternal(const string& commandText)
{
	clearLastCommand();
	lastCommandText = commandText;

	Command parsedCommand = {};
	if (!parseCommandText(commandText, parsedCommand))
	{
		lastExecutionCode = static_cast<int32>(ExecutionCode::parseFailed);
		return false;
	}

	lastCommand = moveValue(parsedCommand);
	const auto registeredCommandIterator = registeredCommandByName.find(lastCommand.name);
	if (registeredCommandIterator == registeredCommandByName.end())
	{
		lastExecutionCode = static_cast<int32>(ExecutionCode::commandNotRegistered);
		return false;
	}

	const RegisteredCommand& registeredCommand = registeredCommandIterator->second;
	const string parameter1 = lastCommand.arguments.size() > 0 ? lastCommand.arguments[0] : "";
	const string parameter2 = lastCommand.arguments.size() > 1 ? lastCommand.arguments[1] : "";
	const string parameter3 = lastCommand.arguments.size() > 2 ? lastCommand.arguments[2] : "";
	lastExecutionCode = registeredCommand.handler(parameter1, parameter2, parameter3);
	return true;
}

bool CLIModule::parseCommandText(const string& commandText, Command& outCommand) const
{
	outCommand = {};

	vector<string> tokens = {};
	string activeToken = {};
	activeToken.reserve(commandText.length());
	const auto flushToken = [&tokens, &activeToken]()
	{
		if (activeToken.empty())
		{
			return;
		}

		tokens.push_back(activeToken);
		activeToken.clear();
	};

	bool quotedText = false;
	for (size_t characterIndex = 0; characterIndex < commandText.length(); ++characterIndex)
	{
		const char character = commandText[characterIndex];
		if (character == '"')
		{
			quotedText = !quotedText;
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
