#pragma once

#include "Engine/Framework/BackendValidation.h"
#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Framework/World.h"
#include "Engine/Window/WindowsWindowObject.h"
#include "Engine/Module/Module.h"

#include "Render/Backends/RenderBackend.h"

struct FrameworkBackendOptions
{
	bool createBackend = true;
	RenderBackendType backendType = RenderBackendType::dx12;
	bool enableDebugLayer = false;
	BackendValidationInjectMode validationInjectMode = BackendValidationInjectMode::none;
};

struct FrameworkInitializeOptions
{
	bool bootstrapWorld = true;
	bool editorUIEnabled = true;
	FrameworkBackendOptions backendOptions = {};
};

template <typename ExitCodeType>
inline int32 resolveFrameworkInitializeExitCode(
	const FrameworkRuntimeExitCode initializeExitCode,
	const ExitCodeType fallbackExitCode)
{
	return initializeExitCode != FrameworkRuntimeExitCode::success
		? static_cast<int32>(initializeExitCode)
		: static_cast<int32>(fallbackExitCode);
}

class Framework
{
public:
	Framework() = default;
	~Framework() = default;

	bool initialize(WindowsWindowObject& windowsWindowObject, const FrameworkInitializeOptions& initializeOptions);
	void shutdown();

	uint32 createWorld(const wstring& worldName);
	bool loadWorld(uint32 worldIndex);
	bool changeWorld(uint32 worldIndex);
	bool unloadWorld(uint32 worldIndex);
	bool loadWorldFromFile(const string& worldFilePath);
	bool saveActiveWorldToFile();
	const string& getActiveWorldFilePath() const;

	World* getWorld(uint32 worldIndex);
	const World* getWorld(uint32 worldIndex) const;
	World* getActiveWorld();
	const World* getActiveWorld() const;
	uint32 getActiveWorldIndex() const;

	bool update();
	FrameworkRuntimeExitCode getRuntimeExitCode() const;
	const FrameworkBackendOptions& getBackendOptions() const;
	WindowsWindowObject* getWindowObject();
	const WindowsWindowObject* getWindowObject() const;
	uint64 getWorldUpdateSerial() const;
	void onWindowResize(uint32 width, uint32 height);
	void setEditorUIEnabled(bool enabled);
	bool isEditorUIEnabled() const;

	void registerModule(const FrameworkInitializeOptions& initializeOptions);
	void addModule(const shared_pointer<Module>& module);

private:
	bool initializeModules();
	void preUpdateModules();
	void postUpdateModules();
	void shutdownModules();
	bool isValidWorldIndex(uint32 worldIndex) const;
	void completeExecution(FrameworkRuntimeExitCode exitCode);

	vector<unique_pointer<World>> worldStorage;
	vector<shared_pointer<Module>> moduleStorage;
	uint32 activeWorldIndex = invalidWorldIndex;

	WindowsWindowObject* windowsWindowObject = nullptr;
	FrameworkBackendOptions backendOptions = {};
	string activeWorldFilePath = {};
	uint64 worldUpdateSerial = 0;

	bool moduleRegistrationCompleted = false;
	bool moduleInitializationCompleted = false;
	bool editorUIEnabled = true;
	FrameworkRuntimeExitCode runtimeExitCode = FrameworkRuntimeExitCode::success;
};
