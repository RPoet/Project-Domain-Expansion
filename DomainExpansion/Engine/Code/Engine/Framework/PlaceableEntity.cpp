#include "Engine/Framework/PlaceableEntity.h"

#include "Engine/Framework/World.h"

PlaceableEntity::PlaceableEntity(memory_resource* componentIndexMemoryResource)
	: Entity(componentIndexMemoryResource)
{
}

void PlaceableEntity::clear()
{
	Entity::clear();
	transform = {};
}

void PlaceableEntity::buildEntityBridgeDynamicData(EntityBridge::DynamicData& dynamicData)
{
	Entity::buildEntityBridgeDynamicData(dynamicData);
	dynamicData.hasTransform = true;
	// TODO: Remove this TEMP_ bridge fallback after Entity caches world transform directly.
	assert(ownerWorld != nullptr && "[PlaceableEntity][Assert] reason=owner_world_missing");
	const bool hasWorldTransform = ownerWorld->TEMP_tryGetEntityWorldTransform(ownerEntityIndex, dynamicData.transform);
	assert(hasWorldTransform && "[PlaceableEntity][Assert] reason=world_transform_build_failed");
	if (!hasWorldTransform)
	{
		dynamicData.transform = transform;
	}
}

void PlaceableEntity::writeAssetProperty(OutputFileStream& fileStream) const
{
	Entity::writeAssetProperty(fileStream);

	XML& xml = XML::get();
	xml.writeProperty(fileStream, transform.positionX);
	xml.writeProperty(fileStream, transform.positionY);
	xml.writeProperty(fileStream, transform.positionZ);
	xml.writeProperty(fileStream, transform.rotationPitch);
	xml.writeProperty(fileStream, transform.rotationYaw);
	xml.writeProperty(fileStream, transform.rotationRoll);
	xml.writeProperty(fileStream, transform.scaleX);
	xml.writeProperty(fileStream, transform.scaleY);
	xml.writeProperty(fileStream, transform.scaleZ);
}

void PlaceableEntity::readAssetProperty(const XMLKeyValueDocument& document)
{
	Entity::readAssetProperty(document);

	XML& xml = XML::get();
	xml.readProperty(document, "deasset", transform.positionX);
	xml.readProperty(document, "deasset", transform.positionY);
	xml.readProperty(document, "deasset", transform.positionZ);
	xml.readProperty(document, "deasset", transform.rotationPitch);
	xml.readProperty(document, "deasset", transform.rotationYaw);
	xml.readProperty(document, "deasset", transform.rotationRoll);
	xml.readProperty(document, "deasset", transform.scaleX);
	xml.readProperty(document, "deasset", transform.scaleY);
	xml.readProperty(document, "deasset", transform.scaleZ);
}
