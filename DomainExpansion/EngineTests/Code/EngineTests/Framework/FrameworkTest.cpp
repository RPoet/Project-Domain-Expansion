#include "EngineTests/Framework/FrameworkTest.h"

#include "Engine/Module/Bridge/BridgeModule.h"
#include "Engine/Module/CLI/CLIModule.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/Input/InputModule.h"
#include "Engine/Module/MeshStreaming/MeshStreaming.h"
#include "Engine/Module/Replay/ReplayModule.h"
#include "Engine/Module/Render/GPUUploader.h"
#include "Engine/Module/Shader/ShaderModule.h"
#include "Engine/Module/ShaderPackage/ShaderPackageModule.h"
#include "Engine/Module/Timer/Timer.h"

bool FrameworkTest::initialize(
	WindowsWindowObject& windowsWindowObject,
	const InitializeOptions& initializeOptions)
{
	addModule(Timer::get());
	addModule(InputModule::get());
	addModule(CLIModule::get());
	addModule(ReplayModule::get());
	addModule(MeshStreaming::get());
	addModule(BridgeModule::get());
	addModule(GPUUploader::get());
	addModule(DiskLoaderModule::get());
	addModule(ShaderModule::get());
	addModule(ShaderPackageModule::get());

	FrameworkInitializeOptions frameworkInitializeOptions = {};
	frameworkInitializeOptions.bootstrapWorld = initializeOptions.bootstrapWorld;
	frameworkInitializeOptions.editorUIEnabled = false;
	frameworkInitializeOptions.backendOptions.createBackend = false;
	return Framework::initialize(windowsWindowObject, frameworkInitializeOptions);
}
