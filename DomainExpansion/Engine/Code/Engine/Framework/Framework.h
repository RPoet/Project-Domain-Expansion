#pragma once

#include "Engine/Framework/BackendValidation.h"
#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Framework/TestFramework.h"
#include "Engine/Framework/World.h"
#include "Engine/Window/WindowsWindowObject.h"
#include "Engine/Module/Module.h"

#include "Render/Backends/RenderBackend.h"

struct FrameworkBackendOptions
{
	RenderBackendType backendType = RenderBackendType::dx12;
	uint32 frameCount = 120;
	bool forceResize = false;
	bool enableDebugLayer = false;
	BackendValidationInjectMode validationInjectMode = BackendValidationInjectMode::none;
};

struct FrameworkInitializeOptions
{
	FrameworkExecutionFlow executionFlow = FrameworkExecutionFlow::worldFlow;
	FrameworkBackendOptions backendOptions = {};
};

class Framework
{
public:
	explicit Framework(FrameworkExecutionFlow executionFlow = FrameworkExecutionFlow::worldFlow);
	~Framework() = default;

	bool initialize(WindowsWindowObject& windowsWindowObject, const FrameworkInitializeOptions& initializeOptions);
	void shutdown();

	void setExecutionFlow(FrameworkExecutionFlow executionFlow);
	FrameworkExecutionFlow getExecutionFlow() const;

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
	bool isExecutionCompleted() const;
	int32 getRuntimeExitCode() const;
	const FrameworkBackendOptions& getBackendOptions() const;
	WindowsWindowObject* getWindowObject();
	const WindowsWindowObject* getWindowObject() const;
	uint64 getWorldUpdateSerial() const;
	bool processBackendValidationFailFast();
	void notifyRenderCommandQueueFlushed();
	void onWindowResize(uint32 width, uint32 height);

	void registerModule();
	void addModule(const shared_pointer<Module>& module);

	void registerTest();
	void addTestCase(unique_pointer<FrameworkTestCase> testCase);
	void clearTestCases();
	bool isTestFlowCompleted() const;
	const FrameworkTestSummary& getTestSummary() const;

private:
	bool initializeModules();
	bool updateTestExecutionFlow();
	bool updateBackendExecutionFlow();
	void preUpdateModules();
	void postUpdateModules();
	void shutdownModules();
	void initializeTestFlow();
	bool tickTestFlow();
	bool initializeBackendFlow();
	bool enqueueBackendRenderFrameCommand();
	bool tickBackendFlow(float deltaTimeSeconds);
	bool ensureEditorCameraFromTemplate(World& world);
	void resetBackendTestState();
	void finalizeTestFlow();
	void finalizeBackendFlow(bool passState);
	bool isValidWorldIndex(uint32 worldIndex) const;

	vector<unique_pointer<World>> worldStorage;
	vector<shared_pointer<Module>> moduleStorage;
	uint32 activeWorldIndex = invalidWorldIndex;

	FrameworkExecutionFlow executionFlow = FrameworkExecutionFlow::worldFlow;
	TestFramework testFramework;
	WindowsWindowObject* windowsWindowObject = nullptr;
	FrameworkBackendOptions backendOptions = {};
	FrameworkBackendTestState backendTestState = {};
	string activeWorldFilePath = {};
	uint64 worldUpdateSerial = 0;

	bool executionCompleted = false;
	bool moduleRegistrationCompleted = false;
	bool moduleInitializationCompleted = false;
	FrameworkRuntimeExitCode runtimeExitCode = FrameworkRuntimeExitCode::success;
};
