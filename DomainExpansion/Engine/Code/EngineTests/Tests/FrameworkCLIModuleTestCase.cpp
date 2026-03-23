#include "EngineTests/Tests/FrameworkCLIModuleTestCase.h"

#include "Engine/Module/MeshParser/MeshParser.h"
#include "Engine/Module/CLI/CLIModule.h"

static bool frameworkCLIModuleCustomCommandExecuted = false;
static constexpr int32 frameworkCLIModuleCustomCommandExecutionCode = 77;

static int32 frameworkCLIModuleCustomCommandHandler(const string& parameter1, const string& parameter2, const string& parameter3)
{
	unused(parameter2);
	unused(parameter3);
	frameworkCLIModuleCustomCommandExecuted = parameter1 == "payload";
	return frameworkCLIModuleCustomCommandExecutionCode;
}

const char* FrameworkCLIModuleTestCase::getTestCaseName() const
{
	return "FrameworkCLIModuleTestCase";
}

bool FrameworkCLIModuleTestCase::beginTest(Framework& framework)
{
	unused(framework);
	return expectCondition(CLIModule::get() != nullptr, "begin: cli module exists");
}

bool FrameworkCLIModuleTestCase::runTest(Framework& framework)
{
	unused(framework);

	shared_pointer<CLIModule> cliModule = CLIModule::get();
	bool runResult = true;
	runResult = expectCondition(
		cliModule != nullptr,
		"run: cli module exists") && runResult;

	frameworkCLIModuleCustomCommandExecuted = false;
	const bool customCommandRegistrationResult = CLIModule::registerCommand(
		"test.custom",
		frameworkCLIModuleCustomCommandHandler);
	runResult = expectCondition(
		customCommandRegistrationResult,
		"run: custom command registers") && runResult;

	const bool customCommandResult = CLIModule::execute("test.custom payload");
	runResult = expectCondition(
		customCommandResult,
		"run: custom command executes") && runResult;
	runResult = expectCondition(
		frameworkCLIModuleCustomCommandExecuted,
		"run: custom command receives command arguments") && runResult;
	runResult = expectCondition(
		cliModule != nullptr && cliModule->getLastExecutionCode() == frameworkCLIModuleCustomCommandExecutionCode,
		"run: custom command code recorded") && runResult;

	const bool importCommandResult = CLIModule::execute("MeshParser.import \"Meshes/Plane.obj\"");
	runResult = expectCondition(
		importCommandResult,
		"run: MeshParser.import command executes") && runResult;
	runResult = expectCondition(
		cliModule != nullptr && cliModule->getLastCommand().name == "MeshParser.import",
		"run: MeshParser.import command name parsed") && runResult;
	runResult = expectCondition(
		cliModule != nullptr
			&& cliModule->getLastCommand().arguments.size() == 1
			&& cliModule->getLastCommand().arguments[0] == "Meshes/Plane.obj",
		"run: MeshParser.import argument parsed") && runResult;
	runResult = expectCondition(
		cliModule != nullptr && cliModule->getLastExecutionCode() == static_cast<int32>(MeshParser::ImportCLIExecutionCode::succeeded),
		"run: MeshParser.import obj parse code recorded") && runResult;

	const bool importFbxResult = CLIModule::execute("MeshParser.import \"Meshes/Character File.fbx\"");
	runResult = expectCondition(
		importFbxResult,
		"run: MeshParser.import fbx command executes") && runResult;
	runResult = expectCondition(
		cliModule != nullptr && cliModule->getLastCommand().name == "MeshParser.import",
		"run: MeshParser.import fbx command name parsed") && runResult;
	runResult = expectCondition(
		cliModule != nullptr
			&& cliModule->getLastCommand().arguments.size() == 1
			&& cliModule->getLastCommand().arguments[0] == "Meshes/Character File.fbx",
		"run: MeshParser.import quoted fbx argument parsed") && runResult;
	runResult = expectCondition(
		cliModule != nullptr && cliModule->getLastExecutionCode() == static_cast<int32>(MeshParser::ImportCLIExecutionCode::fbxNotImplemented),
		"run: MeshParser.import fbx code recorded") && runResult;

	const bool invalidCommandResult = CLIModule::execute("invalid.command");
	runResult = expectCondition(
		!invalidCommandResult,
		"run: unknown command rejected") && runResult;
	runResult = expectCondition(
		cliModule != nullptr && cliModule->getLastExecutionCode() == static_cast<int32>(CLIModule::ExecutionCode::commandNotRegistered),
		"run: unknown command code recorded") && runResult;
	return runResult;
}

bool FrameworkCLIModuleTestCase::endTest(Framework& framework)
{
	unused(framework);
	return true;
}
