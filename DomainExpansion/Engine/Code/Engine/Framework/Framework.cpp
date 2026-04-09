#include "Engine/Framework/Framework.h"

#include "Engine/Assets/AssetLoader.h"
#include "Engine/Module/MeshParser/MeshParser.h"
#include "Engine/Module/Timer/Timer.h"
#include "Engine/Module/TextureParser/TextureParser.h"

static const char* getFrameworkBackendTypeText(const RenderBackendType backendType)
{
	switch (backendType)
	{
	case RenderBackendType::dx12:
		return "dx12";
	case RenderBackendType::vulkan:
		return "vulkan";
	case RenderBackendType::metal:
		return "metal";
	default:
		return "unknown";
	}
}

const FrameworkProfilerOptions& Framework::getProfilerOptions() const
{
	return profilerOptions;
}

bool Framework::initialize(
	WindowsWindowObject& inWindowsWindowObject,
	const FrameworkInitializeOptions& initializeOptions)
{
	PROFILE_SCOPE("startup", "Framework::initialize");

	if (moduleInitializationCompleted)
	{
		for (int32 moduleIndex = static_cast<int32>(moduleStorage.size()) - 1; moduleIndex >= 0; --moduleIndex)
		{
			shared_pointer<Module>& module = moduleStorage[moduleIndex];
			if (module == nullptr)
			{
				continue;
			}

			module->shutdown();
		}

		moduleInitializationCompleted = false;
	}

	activeWorld.reset();
	editorUIEnabled = initializeOptions.editorUIEnabled;
	backendOptions = initializeOptions.backendOptions;
	profilerOptions = initializeOptions.profilerOptions;
	windowsWindowObject = &inWindowsWindowObject;
	runtimeExitCode = FrameworkRuntimeExitCode::success;
	worldUpdateSerial = 0;
	framePerformanceMetrics = {};

	// TO DO : structure framework initilize process this needs to be sevaral step to fit depedency among modules.
	MeshParser::registerCLICommands();
	TextureParser::registerCLICommands();
	if (backendOptions.createBackend)
	{
		if (backendOptions.forceDebugLayer)
		{
			if (!backendOptions.enableDebugLayer)
			{
				output << "[BackendValidation][Policy] flow=world"
					   << " backend=" << getFrameworkBackendTypeText(backendOptions.backendType)
					   << " key=force_debug_layer previous=0 current=1" << lineBreak;
			}

			backendOptions.enableDebugLayer = true;
		}
	}

	const bool hasRegisteredModules = !moduleStorage.empty();
	assert(hasRegisteredModules && "[Framework][Assert] reason=module_not_registered");
	const bool initializedModules = initializeModules();
	assert(initializedModules && "[Framework][Assert] reason=module_initialization_failed");

	if (initializeOptions.bootstrapWorld && getActiveWorld() == nullptr)
	{
		const bool defaultWorldLoaded = loadWorld("Scenes/BistroTest.deasset") != nullptr;
		if (defaultWorldLoaded)
		{
			return true;
		}

		World* editorWorld = createWorld("EditorWorld");
		assert(editorWorld != nullptr && "[Framework][Assert] reason=editor_world_create_failed");
	}

	return true;
}

void Framework::shutdown()
{
	shutdownModules();
	activeWorld.reset();
	backendOptions = {};
	profilerOptions = {};
	windowsWindowObject = nullptr;
	worldUpdateSerial = 0;
	framePerformanceMetrics = {};
	runtimeExitCode = FrameworkRuntimeExitCode::success;
	editorUIEnabled = true;
}

World* Framework::createWorld(const string& worldName)
{
	unique_pointer<World> worldInstance = AssetLoader::get().loadUniqueAsset<World>("Scenes/EditorWorldTemplate.deasset");
	assert(worldInstance != nullptr && "[Framework][Assert] reason=create_world_editor_template_load_failed");
	worldInstance->setName(worldName);
	worldInstance->setAssetPath("");
	activeWorld = moveValue(worldInstance);
	return activeWorld.get();
}

World* Framework::loadWorld(const string& worldAssetPath)
{
	PROFILE_SCOPE_DETAIL("startup", "Framework::loadWorld", worldAssetPath);
	activeWorld = AssetLoader::get().loadUniqueAsset<World>(worldAssetPath);
	assert(activeWorld != nullptr && "[Framework][Assert] reason=world_load_failed");
	return activeWorld.get();
}

bool Framework::unloadWorld()
{
	const bool hadActiveWorld = activeWorld != nullptr;
	activeWorld.reset();
	return hadActiveWorld;
}

bool Framework::saveActiveWorld()
{
	World* loadedActiveWorld = getActiveWorld();
	if (loadedActiveWorld == nullptr || loadedActiveWorld->getAssetPath().empty())
	{
		return false;
	}

	AssetLoader::get().saveWorld(*loadedActiveWorld);
	return true;
}

World* Framework::getActiveWorld()
{
	return activeWorld.get();
}

const World* Framework::getActiveWorld() const
{
	return activeWorld.get();
}

void Framework::update()
{
	preUpdateModules();

	World* activeWorldObject = getActiveWorld();
	if (activeWorldObject != nullptr)
	{
		ScopedTimer worldCpuFrameTimer(framePerformanceMetrics.worldCpuFrameTimeMilliseconds);
		const float deltaTimeSeconds = static_cast<float>(Timer::get()->getDeltaTime());
		activeWorldObject->tick(deltaTimeSeconds);
	}
	else
	{
		framePerformanceMetrics.worldCpuFrameTimeMilliseconds = 0.0f;
	}

	postUpdateModules();
	++worldUpdateSerial;
}

FrameworkRuntimeExitCode Framework::getRuntimeExitCode() const
{
	return runtimeExitCode;
}

const FrameworkBackendOptions& Framework::getBackendOptions() const
{
	return backendOptions;
}

const FramePerformanceMetrics& Framework::getFramePerformanceMetrics() const
{
	return framePerformanceMetrics;
}

WindowsWindowObject* Framework::getWindowObject()
{
	return windowsWindowObject;
}

const WindowsWindowObject* Framework::getWindowObject() const
{
	return windowsWindowObject;
}

uint64 Framework::getWorldUpdateSerial() const
{
	return worldUpdateSerial;
}

void Framework::setEditorUIEnabled(const bool enabled)
{
	editorUIEnabled = enabled;
}

bool Framework::isEditorUIEnabled() const
{
	return editorUIEnabled;
}

void Framework::setRenderFramePerformanceMetrics(
	const float renderWorldCpuFrameTimeMilliseconds,
	const float renderCommandCpuFrameTimeMilliseconds,
	const float gpuFrameTimeMilliseconds)
{
	framePerformanceMetrics.renderWorldCpuFrameTimeMilliseconds = renderWorldCpuFrameTimeMilliseconds;
	framePerformanceMetrics.renderCommandCpuFrameTimeMilliseconds = renderCommandCpuFrameTimeMilliseconds;
	framePerformanceMetrics.gpuFrameTimeMilliseconds = gpuFrameTimeMilliseconds;
}

void Framework::completeExecution(const FrameworkRuntimeExitCode exitCode)
{
	runtimeExitCode = exitCode;
}
