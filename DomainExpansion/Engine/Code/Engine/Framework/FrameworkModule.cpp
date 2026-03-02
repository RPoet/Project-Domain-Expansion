#include "Engine/Framework/Framework.h"
#include "Engine/Module/Asset/MeshStreaming.h"
#include "Engine/Module/Asset/ShaderModule.h"
#include "Engine/Module/Asset/ShaderPackageModule.h"
#include "Engine/Module/Render/GPUUploader.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Module/Timer/Timer.h"
#include "Engine/Module/UI/ImGuiLayerModule.h"

void Framework::registerModule()
{
	if (moduleRegistrationCompleted)
	{
		return;
	}

	addModule(Timer::get());
	addModule(GPUUploader::get());
	addModule(MeshStreaming::get());
	addModule(ShaderModule::get());
	addModule(ShaderPackageModule::get());
	addModule(RenderBackendModule::get());
	addModule(ImGuiLayerModule::get());
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

		if (!module->init(*this))
		{
			error << "Module init failed. module=" << module->getName() << lineBreak;
			return false;
		}
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
