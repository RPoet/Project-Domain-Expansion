#include "Engine/Module/Bridge/BridgeModule.h"

#include "Bridge/CameraBridge.h"
#include "Bridge/EntityBridge.h"
#include "Bridge/MaterialBridge.h"
#include "Bridge/MeshBridge.h"

bool BridgeModule::init(Framework& framework)
{
	unused(framework);
	registerBridge(&CameraBridge::get());
	registerBridge(&EntityBridge::get());
	registerBridge(&MaterialBridge::get());
	registerBridge(&MeshBridge::get());
	return true;
}

void BridgeModule::preUpdate()
{
}

void BridgeModule::postUpdate()
{
	for (uint32 bridgeIndex = 0; bridgeIndex < static_cast<uint32>(bridgeStorage.size()); ++bridgeIndex)
	{
		BaseBridge* bridge = bridgeStorage[bridgeIndex];
		if (bridge == nullptr)
		{
			continue;
		}

		bridge->processFrame();
	}
}

void BridgeModule::shutdown()
{
	bridgeStorage.clear();
}

void BridgeModule::registerBridge(BaseBridge* bridge)
{
	if (bridge == nullptr)
	{
		return;
	}

	for (uint32 bridgeIndex = 0; bridgeIndex < static_cast<uint32>(bridgeStorage.size()); ++bridgeIndex)
	{
		if (bridgeStorage[bridgeIndex] == bridge)
		{
			return;
		}
	}

	bridgeStorage.push_back(bridge);
}

void BridgeModule::unregisterBridge(BaseBridge* bridge)
{
	if (bridge == nullptr)
	{
		return;
	}

	for (uint32 bridgeIndex = 0; bridgeIndex < static_cast<uint32>(bridgeStorage.size()); ++bridgeIndex)
	{
		if (bridgeStorage[bridgeIndex] != bridge)
		{
			continue;
		}

		bridgeStorage.erase(bridgeStorage.begin() + bridgeIndex);
		return;
	}
}
