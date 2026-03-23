#include "EngineTests/Framework/FrameworkBackendTest.h"

#include "Engine/Module/Input/InputModule.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Module/Timer/Timer.h"

bool FrameworkBackendTest::initialize(
	WindowsWindowObject& windowsWindowObject,
	const InitializeOptions& initializeOptions)
{
	addModule(Timer::get());
	addModule(InputModule::get());
	addModule(RenderBackendModule::get());

	FrameworkInitializeOptions frameworkInitializeOptions = {};
	frameworkInitializeOptions.bootstrapWorld = initializeOptions.bootstrapWorld;
	frameworkInitializeOptions.editorUIEnabled = false;
	frameworkInitializeOptions.backendOptions.createBackend = true;
	frameworkInitializeOptions.backendOptions.backendType = initializeOptions.backendType;
	frameworkInitializeOptions.backendOptions.enableDebugLayer = initializeOptions.enableDebugLayer;
	frameworkInitializeOptions.backendOptions.validationInjectMode = initializeOptions.validationInjectMode;
	return Framework::initialize(windowsWindowObject, frameworkInitializeOptions);
}
