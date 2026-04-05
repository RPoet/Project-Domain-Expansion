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
	xml.writeProperty(fileStream, "positionX", transform.positionX);
	xml.writeProperty(fileStream, "positionY", transform.positionY);
	xml.writeProperty(fileStream, "positionZ", transform.positionZ);
	xml.writeProperty(fileStream, "rotationPitch", transform.rotationPitch);
	xml.writeProperty(fileStream, "rotationYaw", transform.rotationYaw);
	xml.writeProperty(fileStream, "rotationRoll", transform.rotationRoll);
	xml.writeProperty(fileStream, "scaleX", transform.scaleX);
	xml.writeProperty(fileStream, "scaleY", transform.scaleY);
	xml.writeProperty(fileStream, "scaleZ", transform.scaleZ);
}

void PlaceableEntity::readAssetProperty(const XMLKeyValueDocument& document)
{
	Entity::readAssetProperty(document);

	XML& xml = XML::get();
	xml.readProperty(document, "deasset.positionX", transform.positionX);
	xml.readProperty(document, "deasset.positionY", transform.positionY);
	xml.readProperty(document, "deasset.positionZ", transform.positionZ);
	xml.readProperty(document, "deasset.rotationPitch", transform.rotationPitch);
	xml.readProperty(document, "deasset.rotationYaw", transform.rotationYaw);
	xml.readProperty(document, "deasset.rotationRoll", transform.rotationRoll);
	xml.readProperty(document, "deasset.scaleX", transform.scaleX);
	xml.readProperty(document, "deasset.scaleY", transform.scaleY);
	xml.readProperty(document, "deasset.scaleZ", transform.scaleZ);
}
