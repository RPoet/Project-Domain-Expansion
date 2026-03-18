#include "Bridge/CameraBridge.h"

CameraBridge::HandleReference CameraBridge::createCameraHandle(const ObjectDesc& objectDesc)
{
	return BridgeType::createObject(objectDesc);
}

bool CameraBridge::isHandleAlive(const PackedHandle packedHandle) const
{
	return BridgeType::isHandleAlive(packedHandle);
}

const CameraBridge::StaticData* CameraBridge::getStaticData(const PackedHandle packedHandle) const
{
	return BridgeType::getStaticProperty(packedHandle);
}

const CameraBridge::DynamicData* CameraBridge::getDynamicData(const PackedHandle packedHandle) const
{
	return BridgeType::getDynamicProperty(packedHandle);
}

void CameraBridge::updateDynamicData(const PackedHandle packedHandle, const DynamicData& dynamicData)
{
	BridgeType::updateObject(packedHandle, dynamicData);
}

void CameraBridge::processFrame()
{
	BridgeType::processFrame();
}
