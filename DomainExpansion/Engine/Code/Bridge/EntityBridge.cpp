#include "Bridge/EntityBridge.h"

EntityBridge::HandleReference EntityBridge::createEntityHandle(const ObjectDesc& objectDesc)
{
	return BridgeType::createObject(objectDesc);
}

bool EntityBridge::isHandleAlive(const HandleReference& handleReference) const
{
	return BridgeType::isHandleAlive(handleReference);
}

bool EntityBridge::isHandleAlive(const PackedHandle packedHandle) const
{
	return BridgeType::isHandleAlive(packedHandle);
}

const EntityBridge::StaticData* EntityBridge::getStaticData(const PackedHandle packedHandle) const
{
	return BridgeType::getStaticProperty(packedHandle);
}

const EntityBridge::StaticData* EntityBridge::getStaticData(const HandleReference& handleReference) const
{
	return BridgeType::getStaticProperty(handleReference);
}

const EntityBridge::DynamicData* EntityBridge::getDynamicData(const PackedHandle packedHandle) const
{
	return BridgeType::getDynamicProperty(packedHandle);
}

const EntityBridge::DynamicData* EntityBridge::getDynamicData(const HandleReference& handleReference) const
{
	return BridgeType::getDynamicProperty(handleReference);
}

void EntityBridge::updateDynamicData(const PackedHandle packedHandle, const DynamicData& dynamicData)
{
	BridgeType::updateObject(packedHandle, dynamicData);
}

void EntityBridge::updateDynamicData(const HandleReference& handleReference, const DynamicData& dynamicData)
{
	BridgeType::updateObject(handleReference, dynamicData);
}

void EntityBridge::processFrame()
{
	BridgeType::processFrame();
}
