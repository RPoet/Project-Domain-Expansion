#include "Engine/Framework/Framework.h"
#include "Engine/Framework/FrameworkFileSystem.h"
#include "Engine/Framework/FrameworkSerialization.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/Input/InputModule.h"
#include "Engine/Module/Timer/Timer.h"
#include "Engine/Module/UI/ImGuiLayerModule.h"

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

static bool loadEditorWorldTemplate(unique_pointer<World>& outWorld, string& outErrorText)
{
	outWorld.reset();
	outErrorText.clear();

	string editorWorldTemplatePath = {};
	if (!frameworkFileSystemResolveEditorWorldTemplateFilePath(editorWorldTemplatePath))
	{
		outErrorText = "editor_world_template_path_resolve_failed";
		return false;
	}

	return frameworkSerializationLoadWorldFromFile(editorWorldTemplatePath, outWorld, outErrorText)
		&& outWorld != nullptr;
}

bool Framework::initialize(
	WindowsWindowObject& inWindowsWindowObject,
	const FrameworkInitializeOptions& initializeOptions)
{
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

	worldStorage.clear();
	activeWorldIndex = invalidWorldIndex;
	editorUIEnabled = initializeOptions.editorUIEnabled;
	backendOptions = initializeOptions.backendOptions;
	windowsWindowObject = &inWindowsWindowObject;
	runtimeExitCode = FrameworkRuntimeExitCode::success;
	activeWorldFilePath.clear();
	worldUpdateSerial = 0;

	if (backendOptions.createBackend)
	{
		if (!backendOptions.enableDebugLayer)
		{
			output << "[BackendValidation][Policy] flow=world"
				   << " backend=" << getFrameworkBackendTypeText(backendOptions.backendType)
				   << " key=force_debug_layer previous=0 current=1" << lineBreak;
		}

		backendOptions.enableDebugLayer = true;
	}

	WindowEventCallbacks windowEventCallbacks = {};
	windowEventCallbacks.onResize = [this](const uint32 width, const uint32 height)
	{
		shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
		diskLoaderModule->TEMP_saveRuntimeWindowResolution(width, height);
		onWindowResize(width, height);
	};
	windowEventCallbacks.onActivationChanged = [](const bool isActive)
	{
		output << "Window activation changed: " << (isActive ? "active" : "inactive") << lineBreak;
	};

	// TO DO : refactor this hook, very ugly.
	windowEventCallbacks.onNativeMessage = [](
		const HandleWindow windowHandle,
		const MessageIdentifier messageIdentifier,
		const MessageFirstParameter firstParameter,
		const MessageSecondParameter secondParameter) -> bool
	{
		shared_pointer<InputModule> inputModule = InputModule::get();
		if (inputModule != nullptr)
		{
			inputModule->handleNativeMessage(
				windowHandle,
				messageIdentifier,
				firstParameter,
				secondParameter);
		}

		shared_pointer<ImGuiLayerModule> imGuiLayerModule = ImGuiLayerModule::get();
		if (imGuiLayerModule == nullptr)
		{
			return false;
		}

		return imGuiLayerModule->processNativeMessage(
			windowHandle,
			messageIdentifier,
			firstParameter,
			secondParameter);
	};
	windowsWindowObject->setEventCallbacks(moveValue(windowEventCallbacks));

	const bool hasRegisteredModules = !moduleStorage.empty();
	assert(hasRegisteredModules && "[Framework][Assert] reason=module_not_registered");
	const bool initializedModules = initializeModules();
	assert(initializedModules && "[Framework][Assert] reason=module_initialization_failed");

	if (initializeOptions.bootstrapWorld && getActiveWorld() == nullptr)
	{
		string defaultWorldPath = {};
		const bool defaultWorldLoaded =
			frameworkFileSystemResolveDefaultWorldFilePath(defaultWorldPath)
			&& loadWorldFromFile(defaultWorldPath);
		if (defaultWorldLoaded)
		{
			return true;
		}

		const uint32 editorWorldIndex = createWorld(L"EditorWorld");
		const bool loadedEditorWorld = loadWorld(editorWorldIndex);
		assert(loadedEditorWorld && "[Framework][Assert] reason=editor_world_load_failed");
	}

	return true;
}

void Framework::shutdown()
{
	shutdownModules();
	worldStorage.clear();
	activeWorldIndex = invalidWorldIndex;
	backendOptions = {};
	windowsWindowObject = nullptr;
	activeWorldFilePath.clear();
	worldUpdateSerial = 0;
	runtimeExitCode = FrameworkRuntimeExitCode::success;
	editorUIEnabled = true;
}

uint32 Framework::createWorld(const wstring& worldName)
{
	unique_pointer<World> worldInstance = nullptr;
	string errorText = {};
	const bool loadedEditorWorldTemplate =
		loadEditorWorldTemplate(worldInstance, errorText)
		&& worldInstance != nullptr;
	assert(loadedEditorWorldTemplate && "[Framework][Assert] reason=create_world_editor_template_load_failed");

	worldInstance->setWorldName(worldName);
	worldStorage.push_back(moveValue(worldInstance));
	return static_cast<uint32>(worldStorage.size() - 1);
}

bool Framework::loadWorld(const uint32 worldIndex)
{
	if (!isValidWorldIndex(worldIndex))
	{
		return false;
	}

	if (worldStorage[worldIndex] == nullptr)
	{
		return false;
	}

	activeWorldIndex = worldIndex;
	activeWorldFilePath.clear();
	return true;
}

bool Framework::changeWorld(const uint32 worldIndex)
{
	return loadWorld(worldIndex);
}

bool Framework::unloadWorld(const uint32 worldIndex)
{
	if (!isValidWorldIndex(worldIndex))
	{
		return false;
	}

	worldStorage[worldIndex].reset();
	if (activeWorldIndex == worldIndex)
	{
		activeWorldIndex = invalidWorldIndex;
		activeWorldFilePath.clear();
	}

	return true;
}

bool Framework::loadWorldFromFile(const string& worldFilePath)
{
	unique_pointer<World> loadedWorld = nullptr;
	string errorText = {};
	const bool loadedWorldFromFile =
		frameworkSerializationLoadWorldFromFile(worldFilePath, loadedWorld, errorText)
		&& loadedWorld != nullptr;
	assert(loadedWorldFromFile && "[Framework][Assert] reason=load_world_from_file_failed");

	worldStorage.push_back(moveValue(loadedWorld));
	const uint32 worldIndex = static_cast<uint32>(worldStorage.size() - 1);
	if (!loadWorld(worldIndex))
	{
		return false;
	}

	activeWorldFilePath = worldFilePath;
	return true;
}

bool Framework::saveActiveWorldToFile()
{
	const World* activeWorld = getActiveWorld();
	if (activeWorld == nullptr || activeWorldFilePath.empty())
	{
		return false;
	}

	string errorText = {};
	const bool savedActiveWorld =
		frameworkSerializationSaveWorldToFile(*activeWorld, activeWorldFilePath, errorText);
	assert(savedActiveWorld && "[Framework][Assert] reason=save_active_world_to_file_failed");

	return true;
}

const string& Framework::getActiveWorldFilePath() const
{
	return activeWorldFilePath;
}

World* Framework::getWorld(const uint32 worldIndex)
{
	if (!isValidWorldIndex(worldIndex))
	{
		return nullptr;
	}

	return worldStorage[worldIndex].get();
}

const World* Framework::getWorld(const uint32 worldIndex) const
{
	if (!isValidWorldIndex(worldIndex))
	{
		return nullptr;
	}

	return worldStorage[worldIndex].get();
}

World* Framework::getActiveWorld()
{
	return getWorld(activeWorldIndex);
}

const World* Framework::getActiveWorld() const
{
	return getWorld(activeWorldIndex);
}

uint32 Framework::getActiveWorldIndex() const
{
	return activeWorldIndex;
}

bool Framework::update()
{
	preUpdateModules();

	const float deltaTimeSeconds = static_cast<float>(Timer::get()->getDeltaTime());
	World* activeWorldObject = getActiveWorld();
	if (activeWorldObject != nullptr)
	{
		activeWorldObject->tick(deltaTimeSeconds);
	}

	postUpdateModules();
	++worldUpdateSerial;

	return true;
}

FrameworkRuntimeExitCode Framework::getRuntimeExitCode() const
{
	return runtimeExitCode;
}

const FrameworkBackendOptions& Framework::getBackendOptions() const
{
	return backendOptions;
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

void Framework::completeExecution(const FrameworkRuntimeExitCode exitCode)
{
	runtimeExitCode = exitCode;
}

bool Framework::isValidWorldIndex(const uint32 worldIndex) const
{
	return worldIndex < static_cast<uint32>(worldStorage.size());
}
