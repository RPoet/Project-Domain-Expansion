#pragma once

#include "Engine/Module/Module.h"

class CLIModule final : public StaticModule<CLIModule>
{
public:
	struct Command
	{
		string name = {};
		vector<string> arguments = {};
	};

	using CommandHandler = function<int32(const vector<string>&)>;

	enum class ExecutionCode : int32
	{
		succeeded = 0,
		parseFailed = -1,
		commandNotRegistered = -2,
	};

	CLIModule()
		: StaticModule("CLIModule")
	{
	}

	bool init(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	static bool execute(const string& commandText);
	static bool registerCommand(const string& commandName, const CommandHandler& commandHandler);

	const Command& getLastCommand() const;
	const string& getLastCommandText() const;
	int32 getLastExecutionCode() const;

private:
	struct RegisteredCommand
	{
		CommandHandler handler = {};
	};

	bool registerCommandInternal(const string& commandName, const CommandHandler& commandHandler);
	bool parseAndDispatchCommandText(
		const string& commandText,
		Command& outCommand,
		int32& outExecutionCode) const;
	bool executeInternal(const string& commandText);
	bool parseCommandText(const string& commandText, Command& outCommand) const;
	void clearLastCommand();

	Command lastCommand = {};
	string lastCommandText = {};
	int32 lastExecutionCode = static_cast<int32>(ExecutionCode::succeeded);
	unordered_map<string, RegisteredCommand> registeredCommandByName = {};
	bool initialized = false;
};
