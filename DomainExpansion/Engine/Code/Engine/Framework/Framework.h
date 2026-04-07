#pragma once

#include "Engine/Framework/BackendValidation.h"
#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Framework/World.h"
#include "Engine/Module/Module.h"
#include "Engine/Profiler/ProfilerBackend.h"
#include "Engine/Profiler/ProfilerScope.h"
#include "Engine/Window/WindowsWindowObject.h"

#include "Render/Backends/RenderBackend.h"

struct FrameworkBackendOptions
{
	bool createBackend = true;
	RenderBackendType backendType = RenderBackendType::dx12;
	bool enableDebugLayer = false;
	BackendValidationInjectMode validationInjectMode = BackendValidationInjectMode::none;
};

struct FrameworkProfilerOptions
{
	ProfilerBackendType backendType = ProfilerBackendType::none;
	string startupCaptureOutputFilePath = {};
};

struct FrameworkInitializeOptions
{
	bool bootstrapWorld = true;
	bool editorUIEnabled = true;
	FrameworkBackendOptions backendOptions = {};
	FrameworkProfilerOptions profilerOptions = {};
};

struct FramePerformanceMetrics
{
	float worldCpuFrameTimeMilliseconds = 0.0f;
	float renderWorldCpuFrameTimeMilliseconds = 0.0f;
	float renderCommandCpuFrameTimeMilliseconds = 0.0f;
	float gpuFrameTimeMilliseconds = 0.0f;
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

	World* createWorld(const string& worldName);
	World* loadWorld(const string& worldAssetPath);
	bool unloadWorld();
	bool saveActiveWorld();

	World* getActiveWorld();
	const World* getActiveWorld() const;

	void update();
	FrameworkRuntimeExitCode getRuntimeExitCode() const;
	const FrameworkBackendOptions& getBackendOptions() const;
	const FrameworkProfilerOptions& getProfilerOptions() const;
	const FramePerformanceMetrics& getFramePerformanceMetrics() const;
	WindowsWindowObject* getWindowObject();
	const WindowsWindowObject* getWindowObject() const;
	uint64 getWorldUpdateSerial() const;
	void onWindowResize(uint32 width, uint32 height);
	void setEditorUIEnabled(bool enabled);
	bool isEditorUIEnabled() const;
	void setRenderFramePerformanceMetrics(float renderWorldCpuFrameTimeMilliseconds, float renderCommandCpuFrameTimeMilliseconds, float gpuFrameTimeMilliseconds);

	void registerModule(const FrameworkInitializeOptions& initializeOptions);
	void addModule(const shared_pointer<Module>& module);

private:
	bool initializeModules();
	void preUpdateModules();
	void postUpdateModules();
	void shutdownModules();
	void completeExecution(FrameworkRuntimeExitCode exitCode);

	vector<shared_pointer<Module>> moduleStorage;
	unique_pointer<World> activeWorld = nullptr;

	WindowsWindowObject* windowsWindowObject = nullptr;
	FrameworkBackendOptions backendOptions = {};
	FrameworkProfilerOptions profilerOptions = {};
	FramePerformanceMetrics framePerformanceMetrics = {};
	uint64 worldUpdateSerial = 0;

	bool moduleRegistrationCompleted = false;
	bool moduleInitializationCompleted = false;
	bool editorUIEnabled = true;
	FrameworkRuntimeExitCode runtimeExitCode = FrameworkRuntimeExitCode::success;
};
