#include "Engine/Framework/PlaceableEntity.h"

PlaceableEntity::PlaceableEntity(memory_resource* componentIndexMemoryResource)
	: Entity(componentIndexMemoryResource)
{
}

void PlaceableEntity::buildEntityBridgeDynamicData(EntityBridge::DynamicData& dynamicData)
{
	Entity::buildEntityBridgeDynamicData(dynamicData);
	dynamicData.hasTransform = true;
	dynamicData.transform = transform;
}
