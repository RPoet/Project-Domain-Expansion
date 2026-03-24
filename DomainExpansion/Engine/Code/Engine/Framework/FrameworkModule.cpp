#include "Engine/Framework/Framework.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/MeshStreaming/MeshStreaming.h"
#include "Engine/Module/Shader/ShaderModule.h"
#include "Engine/Module/ShaderPackage/ShaderPackageModule.h"
#include "Engine/Module/Bridge/BridgeModule.h"
#include "Engine/Module/CLI/CLIModule.h"
#include "Engine/Module/Input/InputModule.h"
#include "Engine/Module/Render/GPUUploader.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Module/Timer/Timer.h"
#include "Engine/Module/UI/ImGuiLayerModule.h"
#include "Engine/Module/XML/XMLModule.h"

void Framework::registerModule(const FrameworkInitializeOptions& initializeOptions)
{
	if (moduleRegistrationCompleted)
	{
		return;
	}

	editorUIEnabled = initializeOptions.editorUIEnabled;

	addModule(Timer::get());
	addModule(InputModule::get());
	addModule(CLIModule::get());
	addModule(MeshStreaming::get());
	addModule(BridgeModule::get());
	addModule(RenderBackendModule::get());
	addModule(GPUUploader::get());
	addModule(DiskLoaderModule::get());
	addModule(XMLModule::get());
	addModule(ShaderModule::get());
	addModule(ShaderPackageModule::get());
	if (editorUIEnabled)
	{
		addModule(ImGuiLayerModule::get());
	}
	moduleRegistrationCompleted = true;
}

void Framework::addModule(const shared_pointer<Module>& module)
{
	if (module == nullptr)
	{
		return;
	}

	for (uint32 moduleIndex = 0; moduleIndex < static_cast<uint32>(moduleStorage.size()); ++moduleIndex)
	{
		const shared_pointer<Module>& storedModule = moduleStorage[moduleIndex];
		if (storedModule == nullptr)
		{
			continue;
		}

		if (storedModule.get() == module.get())
		{
			return;
		}

		if (storedModule->getName() == module->getName())
		{
			return;
		}
	}

	moduleStorage.push_back(module);
}

bool Framework::initializeModules()
{
	if (moduleInitializationCompleted)
	{
		return true;
	}

	for (uint32 moduleIndex = 0; moduleIndex < static_cast<uint32>(moduleStorage.size()); ++moduleIndex)
	{
		shared_pointer<Module>& module = moduleStorage[moduleIndex];
		if (module == nullptr)
		{
			continue;
		}

		const bool initializedModule = module->init(*this);
		assert(initializedModule && "[Framework][Assert] reason=module_init_failed");
	}

	moduleInitializationCompleted = true;
	return true;
}

void Framework::preUpdateModules()
{
	if (!moduleInitializationCompleted)
	{
		return;
	}

	for (uint32 moduleIndex = 0; moduleIndex < static_cast<uint32>(moduleStorage.size()); ++moduleIndex)
	{
		shared_pointer<Module>& module = moduleStorage[moduleIndex];
		if (module == nullptr)
		{
			continue;
		}

		module->preUpdate();
	}
}

void Framework::postUpdateModules()
{
	if (!moduleInitializationCompleted)
	{
		return;
	}

	for (uint32 moduleIndex = 0; moduleIndex < static_cast<uint32>(moduleStorage.size()); ++moduleIndex)
	{
		shared_pointer<Module>& module = moduleStorage[moduleIndex];
		if (module == nullptr)
		{
			continue;
		}

		module->postUpdate();
	}
}

void Framework::shutdownModules()
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
	}

	moduleStorage.clear();
	moduleRegistrationCompleted = false;
	moduleInitializationCompleted = false;
}
