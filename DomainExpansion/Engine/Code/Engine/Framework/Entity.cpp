#include "Engine/Framework/Entity.h"

#include "Engine/Framework/Component.h"
#include "Engine/Framework/World.h"

static unordered_map<string, EntityFactoryFunction>& getEntityFactoryByTypeName()
{
	static unordered_map<string, EntityFactoryFunction> entityFactoryByTypeName;
	return entityFactoryByTypeName;
}

static bool isSameEntityBridgeDynamicData(
	const EntityBridge::DynamicData& left,
	const EntityBridge::DynamicData& right)
{
	return left.entityIndex == right.entityIndex
		&& left.active == right.active
		&& left.hasTransform == right.hasTransform
		&& left.transform == right.transform;
}

EntityFactoryRegistration::EntityFactoryRegistration(const char* assetTypeName, const EntityFactoryFunction createFactory)
{
	assert(assetTypeName != nullptr && "[Entity][Assert] reason=entity_factory_asset_type_name_missing");
	assert(createFactory != nullptr && "[Entity][Assert] reason=entity_factory_create_function_missing");
	getEntityFactoryByTypeName().emplace(assetTypeName, createFactory);
}

unique_pointer<Entity> Entity::createByAssetTypeName(const string& assetTypeName)
{
	TRACE_EVENT("framework", "Entity::createByAssetTypeName");
	const auto foundFactory = getEntityFactoryByTypeName().find(assetTypeName);
	assert(foundFactory != getEntityFactoryByTypeName().end() && "[Entity][Assert] reason=entity_factory_missing");
	return foundFactory->second();
}

Entity::Entity(memory_resource* componentIndexMemoryResource)
	: componentIndices(componentIndexMemoryResource != nullptr ? componentIndexMemoryResource : getDefaultMemoryResource())
{
}

void Entity::clear()
{
	Asset::clear();
	parentEntityAssetPath.clear();
	active = true;
}

void Entity::initialize()
{
	EntityBridge::DynamicData dynamicData = {};
	buildEntityBridgeDynamicData(dynamicData);
	if (!entityBridgeHandle.isValid())
	{
		EntityBridge::ObjectDesc entityObjectDesc = {};
		entityObjectDesc.dynamicProperty = dynamicData;
		entityBridgeHandle = EntityBridge::get().createEntityHandle(entityObjectDesc);
		assert(entityBridgeHandle.isValid());
		return;
	}

	const EntityBridge::DynamicData* currentDynamicData = EntityBridge::get().getDynamicData(entityBridgeHandle);
	if (currentDynamicData == nullptr || isSameEntityBridgeDynamicData(*currentDynamicData, dynamicData))
	{
		return;
	}

	EntityBridge::get().updateDynamicData(entityBridgeHandle, dynamicData);
}

void Entity::buildEntityBridgeDynamicData(EntityBridge::DynamicData& dynamicData)
{
	dynamicData = {};
	dynamicData.entityIndex = ownerEntityIndex;
	dynamicData.active = active;

	if (ownerWorld == nullptr)
	{
		return;
	}
}

void Entity::writeAssetProperty(OutputFileStream& fileStream) const
{
	XML& xml = XML::get();
	xml.writeProperty(fileStream, "active", active);

	string serializedParentEntityAssetPath = parentEntityAssetPath;
	if (ownerWorld != nullptr && parentEntityIndex != invalidEntityIndex)
	{
		const Entity* parentEntity = ownerWorld->getEntityByIndex(parentEntityIndex);
		assert(parentEntity != nullptr && "[Entity][Assert] reason=parent_entity_missing");
		assert(!parentEntity->getAssetPath().empty() && "[Entity][Assert] reason=parent_entity_asset_path_missing");
		serializedParentEntityAssetPath = parentEntity->getAssetPath();
	}

	xml.writeProperty(fileStream, "parentAssetPath", serializedParentEntityAssetPath);
}

void Entity::readAssetProperty(const XMLKeyValueDocument& document)
{
	XML& xml = XML::get();
	xml.readProperty(document, "deasset.active", active);
	xml.readProperty(document, "deasset.parentAssetPath", parentEntityAssetPath);
}

void Entity::requestEntityBridgeUpdate()
{
	if (!entityBridgeHandle.isValid())
	{
		return;
	}

	EntityBridge::DynamicData nextDynamicData = {};
	buildEntityBridgeDynamicData(nextDynamicData);

	const EntityBridge::DynamicData* currentDynamicData = EntityBridge::get().getDynamicData(entityBridgeHandle);
	if (currentDynamicData != nullptr && isSameEntityBridgeDynamicData(*currentDynamicData, nextDynamicData))
	{
		return;
	}

	EntityBridge::get().updateDynamicData(entityBridgeHandle, nextDynamicData);
}

void Entity::tickComponents(const float deltaTimeSeconds)
{
	if (ownerWorld == nullptr)
	{
		return;
	}

	for (uint32 componentArrayIndex = 0; componentArrayIndex < static_cast<uint32>(componentIndices.size()); ++componentArrayIndex)
	{
		const uint32 componentIndex = componentIndices[componentArrayIndex];
		Component* component = ownerWorld->getComponentByIndex(componentIndex);
		if (component == nullptr)
		{
			continue;
		}

		component->tick(deltaTimeSeconds);
	}
}

void Entity::tick(const float deltaTimeSeconds)
{
	tickComponents(deltaTimeSeconds);
	requestEntityBridgeUpdate();
}

bool Entity::addComponent(unique_pointer<Component> component)
{
	assert(ownerWorld != nullptr && "[Entity][Assert] reason=owner_world_missing");
	return ownerWorld->attachComponent(ownerEntityIndex, moveValue(component));
}

bool Entity::removeComponent(const uint32 componentIndex)
{
	assert(ownerWorld != nullptr && "[Entity][Assert] reason=owner_world_missing");
	return ownerWorld->removeComponent(ownerEntityIndex, componentIndex);
}

World* Entity::getOwnerWorld()
{
	return ownerWorld;
}

const World* Entity::getOwnerWorld() const
{
	return ownerWorld;
}

uint32 Entity::getEntityIndex() const
{
	return ownerEntityIndex;
}

BridgeHandle Entity::getEntityHandle() const
{
	return entityBridgeHandle.getPackedHandle();
}

uint32 Entity::getComponentCount() const
{
	return static_cast<uint32>(componentIndices.size());
}

uint32 Entity::getComponentIndex(const uint32 componentArrayIndex) const
{
	if (componentArrayIndex >= static_cast<uint32>(componentIndices.size()))
	{
		return invalidComponentIndex;
	}

	return componentIndices[componentArrayIndex];
}

uint32 Entity::getParentEntityIndex() const
{
	return parentEntityIndex;
}

uint32 Entity::getFirstChildEntityIndex() const
{
	return firstChildEntityIndex;
}

uint32 Entity::getNextSiblingEntityIndex() const
{
	return nextSiblingEntityIndex;
}

const string& Entity::getParentEntityAssetPath() const
{
	return parentEntityAssetPath;
}

bool Entity::isActive() const
{
	return active;
}

void Entity::setActive(const bool active)
{
	if (this->active == active)
	{
		return;
	}

	this->active = active;
	requestEntityBridgeUpdate();
}
