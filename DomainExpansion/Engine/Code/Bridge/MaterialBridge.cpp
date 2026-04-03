#include "Bridge/MaterialBridge.h"

MaterialBridge::HandleReference MaterialBridge::createMaterialHandle(const ObjectDesc& objectDesc)
{
	return BridgeType::createObject(objectDesc);
}

bool MaterialBridge::isHandleAlive(const PackedHandle packedHandle) const
{
	return BridgeType::isHandleAlive(packedHandle);
}

const MaterialBridge::StaticData* MaterialBridge::getStaticData(const PackedHandle packedHandle) const
{
	return BridgeType::getStaticProperty(packedHandle);
}

void MaterialBridge::processFrame()
{
	BridgeType::processFrame();
}
