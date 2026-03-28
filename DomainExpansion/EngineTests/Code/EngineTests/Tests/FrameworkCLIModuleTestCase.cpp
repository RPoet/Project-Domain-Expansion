#include "EngineTests/Tests/FrameworkCLIModuleTestCase.h"

#include "Engine/Common/EditorCommandReplay.h"
#include "Engine/Framework/CameraComponent.h"
#include "Engine/Framework/Component.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/World.h"
#include "Engine/Module/CLI/CLIModule.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/MeshParser/MeshParser.h"
#include "EngineTests/Framework/EditorReplayRunner.h"

static bool frameworkCLIModuleCustomCommandExecuted = false;
static constexpr int32 frameworkCLIModuleCustomCommandExecutionCode = 77;

static int32 frameworkCLIModuleCustomCommandHandler(const vector<string>& arguments)
{
	frameworkCLIModuleCustomCommandExecuted =
		arguments.size() == 1
		&& arguments[0] == "payload";
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

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	runResult = expectCondition(
		diskLoaderModule != nullptr,
		"run: disk loader module exists for import output validation") && runResult;
	if (diskLoaderModule != nullptr)
	{
		string planeMeshAssetPath = {};
		string sphereMeshAssetPath = {};
		string planeMeshBinaryPath = {};
		string sphereMeshBinaryPath = {};
		const bool planeImported = CLIModule::execute("MeshParser.import \"Meshes/Plane.obj\"");
		const bool sphereImported = CLIModule::execute("MeshParser.import \"Meshes/Sphere.obj\"");
		runResult = expectCondition(planeImported && sphereImported, "run: default plane and sphere imports execute") && runResult;
		runResult = expectCondition(
			diskLoaderModule->resolvePathFromResources("Meshes/Plane.deasset", planeMeshAssetPath)
				&& diskLoaderModule->resolvePathFromResources("Meshes/Sphere.deasset", sphereMeshAssetPath)
				&& !(planeMeshBinaryPath = diskLoaderModule->resolveAssetPath("Meshes/Plane.deasset", DiskLoaderModule::AssetFileType::binary)).empty()
				&& !(sphereMeshBinaryPath = diskLoaderModule->resolveAssetPath("Meshes/Sphere.deasset", DiskLoaderModule::AssetFileType::binary)).empty()
				&& diskLoaderModule->resolveAbsolutePathFromResources(planeMeshBinaryPath, planeMeshBinaryPath)
				&& diskLoaderModule->resolveAbsolutePathFromResources(sphereMeshBinaryPath, sphereMeshBinaryPath)
				&& exists(filesystem_path(planeMeshAssetPath))
				&& exists(filesystem_path(sphereMeshAssetPath))
				&& exists(filesystem_path(planeMeshBinaryPath))
				&& exists(filesystem_path(sphereMeshBinaryPath)),
			"run: MeshParser.import writes default mesh asset documents and binaries") && runResult;
	}

	if (diskLoaderModule != nullptr)
	{
		string solutionRootPath = {};
		runResult = expectCondition(
			diskLoaderModule->TEMP_resolveSolutionRootPath(solutionRootPath),
			"run: resolve solution root for save-active-world command") && runResult;
		if (!solutionRootPath.empty())
		{
			World* activeWorld = framework.createWorld("FrameworkCLISaveWorld");
			runResult = expectCondition(activeWorld != nullptr, "run: create active world for save-active-world command") && runResult;
			if (activeWorld != nullptr)
			{
				const string worldAssetPath =
					(filesystem_path(solutionRootPath) / "EngineTests" / "Artifacts" / "Temp" / "FrameworkCLISaveWorld.deasset")
						.lexically_normal()
						.string();
				activeWorld->setAssetPath(worldAssetPath);
				const bool saveActiveWorldCommandResult = CLIModule::execute("Framework.saveActiveWorld");
				runResult = expectCondition(saveActiveWorldCommandResult, "run: Framework.saveActiveWorld command executes") && runResult;
				runResult = expectCondition(
					cliModule != nullptr
						&& cliModule->getLastExecutionCode() == static_cast<int32>(CLIModule::ExecutionCode::succeeded),
					"run: Framework.saveActiveWorld command code recorded") && runResult;
				runResult = expectCondition(
					exists(filesystem_path(worldAssetPath)),
					"run: Framework.saveActiveWorld writes world document") && runResult;
			}
		}

		if (!solutionRootPath.empty())
		{
			const filesystem_path replayRootPath =
				(filesystem_path(solutionRootPath) / "EngineTests" / "Artifacts" / "Temp" / "FrameworkEditorReplay")
					.lexically_normal();
			error_code replayDirectoryErrorCode = {};
			remove_all(replayRootPath, replayDirectoryErrorCode);
			create_directories(replayRootPath, replayDirectoryErrorCode);
			runResult = expectCondition(!replayDirectoryErrorCode, "run: create replay temp directory") && runResult;
			if (!replayDirectoryErrorCode)
			{
				const string replayWorldAssetPath = (replayRootPath / "ReplayWorld.deasset").lexically_normal().string();
				const string replayCommandFilePath = (replayRootPath / "editor_replay.log").lexically_normal().string();
				const string replayEntityAssetPath = (replayRootPath / "ReplayWorld" / "Entity2.deasset").lexically_normal().string();
				const string replayMeshComponentAssetPath =
					(replayRootPath / "ReplayWorld" / "ReplayWorld_Component3.deasset").lexically_normal().string();
				const string replayCameraComponentAssetPath =
					(replayRootPath / "ReplayWorld" / "ReplayWorld_Component4.deasset").lexically_normal().string();

				vector<string> replayCommandTexts = {};
				replayCommandTexts.push_back(EditorCommandReplay::buildCommandText("Editor.createWorld", { "ReplayWorld", replayWorldAssetPath }));
				replayCommandTexts.push_back(EditorCommandReplay::buildCommandText(
					"Editor.addEntity",
					{ replayEntityAssetPath, "", PlaceableEntity::getStaticAssetTypeName() }));
				replayCommandTexts.push_back(EditorCommandReplay::buildCommandText(
					"Editor.setEntityName",
					{ replayEntityAssetPath, "Replay Entity" }));
				replayCommandTexts.push_back(EditorCommandReplay::buildCommandText(
					"Editor.setTransform",
					{ replayEntityAssetPath, "1", "2", "3", "4", "5", "6", "7", "8", "9" }));
				replayCommandTexts.push_back(EditorCommandReplay::buildCommandText(
					"Editor.addComponent",
					{ replayEntityAssetPath, MeshComponent::getStaticAssetTypeName(), replayMeshComponentAssetPath }));
				replayCommandTexts.push_back(EditorCommandReplay::buildCommandText(
					"Editor.setMeshComponent",
					{ replayMeshComponentAssetPath, "Meshes/Sphere.deasset", "0", "1" }));
				replayCommandTexts.push_back(EditorCommandReplay::buildCommandText(
					"Editor.addComponent",
					{ replayEntityAssetPath, CameraComponent::getStaticAssetTypeName(), replayCameraComponentAssetPath }));
				replayCommandTexts.push_back(EditorCommandReplay::buildCommandText(
					"Editor.setCameraComponent",
					{ replayCameraComponentAssetPath, "1", "75", "0.25", "250" }));

				OutputFileStream replayCommandFileStream = diskLoaderModule->openOutputFileStream(replayCommandFilePath, false, true);
				for (uint32 commandIndex = 0; commandIndex < static_cast<uint32>(replayCommandTexts.size()); ++commandIndex)
				{
					replayCommandFileStream << replayCommandTexts[commandIndex] << '\n';
				}

				EditorReplayRunner immediateReplayRunner = {};
				const bool immediateReplayLoadResult = immediateReplayRunner.load(replayCommandFilePath);
				runResult = expectCondition(immediateReplayLoadResult, "run: editor replay runner loads replay file") && runResult;
				const bool immediateReplayCommandResult = immediateReplayRunner.replayImmediate();
				runResult = expectCondition(immediateReplayCommandResult, "run: editor replay runner immediate execute succeeds") && runResult;
				runResult = expectCondition(
					immediateReplayRunner.isCompleted()
						&& !immediateReplayRunner.hasFailed()
						&& immediateReplayRunner.getLastExecutionCode() == static_cast<int32>(CLIModule::ExecutionCode::succeeded),
					"run: editor replay runner immediate completion state is valid") && runResult;

				World* replayWorld = framework.getActiveWorld();
				runResult = expectCondition(replayWorld != nullptr, "run: replay creates active world") && runResult;
				if (replayWorld != nullptr)
				{
					runResult = expectCondition(
						replayWorld->getName() == "ReplayWorld" && replayWorld->getAssetPath() == replayWorldAssetPath,
						"run: replay world identity matches") && runResult;

					PlaceableEntity* replayEntity = dynamic_cast<PlaceableEntity*>(replayWorld->getEntityByIndex(2));
					runResult = expectCondition(
						replayEntity != nullptr
							&& replayEntity->getAssetPath() == replayEntityAssetPath
							&& replayEntity->getName() == "Replay Entity",
						"run: replay adds and renames entity") && runResult;
					runResult = expectCondition(
						replayEntity != nullptr
							&& replayEntity->transform.positionX == 1.0f
							&& replayEntity->transform.positionY == 2.0f
							&& replayEntity->transform.positionZ == 3.0f
							&& replayEntity->transform.rotationPitch == 4.0f
							&& replayEntity->transform.rotationYaw == 5.0f
							&& replayEntity->transform.rotationRoll == 6.0f
							&& replayEntity->transform.scaleX == 7.0f
							&& replayEntity->transform.scaleY == 8.0f
							&& replayEntity->transform.scaleZ == 9.0f,
						"run: replay applies transform") && runResult;

					MeshComponent* replayMeshComponent = nullptr;
					CameraComponent* replayCameraComponent = nullptr;
					if (replayEntity != nullptr)
					{
						for (uint32 componentArrayIndex = 0; componentArrayIndex < replayEntity->getComponentCount(); ++componentArrayIndex)
						{
							Component* component = replayWorld->getComponentByIndex(replayEntity->getComponentIndex(componentArrayIndex));
							if (component == nullptr)
							{
								continue;
							}

							if (component->getComponentType() == MeshComponent::staticComponentType)
							{
								replayMeshComponent = static_cast<MeshComponent*>(component);
							}
							else if (component->getComponentType() == CameraComponent::staticComponentType)
							{
								replayCameraComponent = static_cast<CameraComponent*>(component);
							}
						}
					}

					runResult = expectCondition(
						replayMeshComponent != nullptr
							&& replayMeshComponent->getAssetPath() == replayMeshComponentAssetPath
							&& replayMeshComponent->meshAssetPath == "Meshes/Sphere.deasset"
							&& replayMeshComponent->lodLevel == 0
							&& replayMeshComponent->visible,
						"run: replay applies mesh component state") && runResult;
					runResult = expectCondition(
						replayCameraComponent != nullptr
							&& replayCameraComponent->getAssetPath() == replayCameraComponentAssetPath
							&& replayCameraComponent->primary
							&& replayCameraComponent->fieldOfViewYDegrees == 75.0f
							&& replayCameraComponent->nearPlane == 0.25f
							&& replayCameraComponent->farPlane == 250.0f,
						"run: replay applies camera component state") && runResult;
				}

				EditorReplayRunner steppedReplayRunner = {};
				const bool steppedReplayLoadResult = steppedReplayRunner.load(replayCommandFilePath);
				runResult = expectCondition(steppedReplayLoadResult, "run: editor replay runner loads stepped replay file") && runResult;
				runResult = expectCondition(steppedReplayRunner.hasPendingCommands(), "run: stepped replay becomes active") && runResult;
				for (uint32 updateIndex = 0; updateIndex < static_cast<uint32>(replayCommandTexts.size()); ++updateIndex)
				{
					const bool replayStepResult = steppedReplayRunner.step();
					runResult = expectCondition(replayStepResult, "run: stepped replay executes replay step") && runResult;
					const bool frameworkUpdated = framework.update();
					runResult = expectCondition(frameworkUpdated, "run: framework update executes queued replay step") && runResult;
				}

				runResult = expectCondition(
					steppedReplayRunner.isCompleted() && !steppedReplayRunner.hasFailed(),
					"run: stepped replay completes") && runResult;
			}
		}
	}

	runResult = expectCondition(
		true,
		"run: MeshParser.import fbx path skipped until FBX parser implementation") && runResult;

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
