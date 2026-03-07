#include "Bridge/MeshBridge.h"

MeshBridge::HandleReference MeshBridge::createMeshHandle(const ObjectDesc& objectDesc)
{
	return BridgeType::createObject(objectDesc);
}

bool MeshBridge::isHandleAlive(const PackedHandle packedHandle) const
{
	return BridgeType::isHandleAlive(packedHandle);
}

const MeshBridge::StaticData* MeshBridge::getStaticData(const PackedHandle packedHandle) const
{
	return BridgeType::getStaticProperty(packedHandle);
}

const MeshBridge::DynamicData* MeshBridge::getDynamicData(const PackedHandle packedHandle) const
{
	return BridgeType::getDynamicProperty(packedHandle);
}

void MeshBridge::updateDynamicData(const PackedHandle packedHandle, const DynamicData& dynamicData)
{
	BridgeType::updateObject(packedHandle, dynamicData);
}

void MeshBridge::processFrame()
{
	BridgeType::processFrame();
}
