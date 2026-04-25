#include "Engine/Framework/World.h"

// TODO: Replace this TEMP_ hierarchy traversal with cached subtree world-transform propagation.
static float4x4 TEMP_buildEntityLocalMatrix(const Entity& entity)
{
	if (entity.getEntityType() != EntityType::placeableEntity)
	{
		return buildIdentityMatrix4x4();
	}

	const PlaceableEntity& placeableEntity = static_cast<const PlaceableEntity&>(entity);
	return TEMP_buildTransformMatrix4x4(placeableEntity.transform);
}

World::World(const string& worldName)
{
	setName(worldName);
}

void World::clear()
{
	Asset::clear();
	componentStorage.clear();
	componentOwnerIndices.clear();
	entityStorage.clear();
	traversalEntityIndices.clear();
	componentIndexPoolResource.release();
}

void World::writeAssetProperty(OutputFileStream& fileStream) const
{
	XML& xml = XML::get();
	vector<string> entityAssetPaths = {};
	entityAssetPaths.reserve(entityStorage.size());
	for (uint32 entityIndex = 0; entityIndex < static_cast<uint32>(entityStorage.size()); ++entityIndex)
	{
		const Entity* entity = entityStorage[entityIndex].get();
		assert(entity != nullptr && "[World][Assert] reason=entity_missing");
		assert(!entity->getAssetPath().empty() && "[World][Assert] reason=entity_asset_path_missing");
		entityAssetPaths.push_back(entity->getAssetPath());
	}
	xml.writePropertyArray(fileStream, "Entities", entityAssetPaths);

	vector<string> componentAssetPaths = {};
	componentAssetPaths.reserve(componentStorage.size());
	for (uint32 componentIndex = 0; componentIndex < static_cast<uint32>(componentStorage.size()); ++componentIndex)
	{
		const Component* component = componentStorage[componentIndex].get();
		assert(component != nullptr && "[World][Assert] reason=component_missing");
		assert(!component->getAssetPath().empty() && "[World][Assert] reason=component_asset_path_missing");
		componentAssetPaths.push_back(component->getAssetPath());
	}
	xml.writePropertyArray(fileStream, "Components", componentAssetPaths);
}

void World::readAssetProperty(const XMLKeyValueDocument& document)
{
	unused(document);
}

uint32 World::createEntity()
{
	return createEntity(true);
}

uint32 World::createPlaceableEntity()
{
	return createPlaceableEntity(true);
}

uint32 World::createEntity(const bool initializeEntity)
{
	return addEntityObject(unique_pointer<Entity>(new Entity(&componentIndexPoolResource)), initializeEntity);
}

uint32 World::createPlaceableEntity(const bool initializeEntity)
{
	return addEntityObject(unique_pointer<Entity>(new PlaceableEntity(&componentIndexPoolResource)), initializeEntity);
}

uint32 World::addEntityObject(unique_pointer<Entity> entity, const bool initializeEntity)
{
	assert(entity != nullptr);
	entityStorage.push_back(moveValue(entity));
	const uint32 entityIndex = static_cast<uint32>(entityStorage.size() - 1);
	Entity* createdEntity = entityStorage[entityIndex].get();
	if (createdEntity != nullptr)
	{
		createdEntity->ownerWorld = this;
		createdEntity->ownerEntityIndex = entityIndex;
		if (initializeEntity)
		{
			createdEntity->initialize();
		}
	}
	return entityIndex;
}

bool World::addChildEntity(const uint32 parentEntityIndex, const uint32 childEntityIndex)
{
	if (!isValidEntityIndex(parentEntityIndex) || !isValidEntityIndex(childEntityIndex))
	{
		return false;
	}

	if (parentEntityIndex == childEntityIndex)
	{
		return false;
	}

	Entity* parentEntity = getEntity(parentEntityIndex);
	Entity* childEntity = getEntity(childEntityIndex);
	if (parentEntity == nullptr || childEntity == nullptr)
	{
		return false;
	}

	if (childEntity->parentEntityIndex != invalidEntityIndex)
	{
		return false;
	}

	return reparentEntity(childEntityIndex, parentEntityIndex);
}

bool World::reparentEntity(const uint32 childEntityIndex, const uint32 parentEntityIndex)
{
	if (!isValidEntityIndex(childEntityIndex))
	{
		return false;
	}

	if (parentEntityIndex != invalidEntityIndex && !isValidEntityIndex(parentEntityIndex))
	{
		return false;
	}

	if (parentEntityIndex == childEntityIndex)
	{
		return false;
	}

	Entity* childEntity = getEntity(childEntityIndex);
	Entity* parentEntity = parentEntityIndex != invalidEntityIndex ? getEntity(parentEntityIndex) : nullptr;
	if (childEntity == nullptr || (parentEntityIndex != invalidEntityIndex && parentEntity == nullptr))
	{
		return false;
	}

	if (childEntity->parentEntityIndex == parentEntityIndex)
	{
		return true;
	}

	uint32 parentTraversalEntityIndex = parentEntityIndex;
	uint32 parentTraversalCount = 0;
	const uint32 maxParentTraversalCount = static_cast<uint32>(entityStorage.size());
	while (parentTraversalEntityIndex != invalidEntityIndex && parentTraversalCount < maxParentTraversalCount)
	{
		if (parentTraversalEntityIndex == childEntityIndex)
		{
			return false;
		}

		const Entity* parentTraversalEntity = getEntity(parentTraversalEntityIndex);
		if (parentTraversalEntity == nullptr)
		{
			break;
		}

		parentTraversalEntityIndex = parentTraversalEntity->parentEntityIndex;
		++parentTraversalCount;
	}

	uint32 lastChildSiblingEntityIndex = invalidEntityIndex;
	if (parentEntity != nullptr)
	{
		uint32 siblingEntityIndex = parentEntity->firstChildEntityIndex;
		uint32 siblingCount = 0;
		const uint32 maxSiblingCount = static_cast<uint32>(entityStorage.size());
		while (siblingEntityIndex != invalidEntityIndex && siblingCount < maxSiblingCount)
		{
			Entity* siblingEntity = getEntity(siblingEntityIndex);
			if (siblingEntity == nullptr)
			{
				return false;
			}

			lastChildSiblingEntityIndex = siblingEntityIndex;
			siblingEntityIndex = siblingEntity->nextSiblingEntityIndex;
			++siblingCount;
		}
	}

	if (childEntity->parentEntityIndex != invalidEntityIndex)
	{
		Entity* previousParentEntity = getEntity(childEntity->parentEntityIndex);
		if (previousParentEntity == nullptr)
		{
			return false;
		}

		if (previousParentEntity->firstChildEntityIndex == childEntityIndex)
		{
			previousParentEntity->firstChildEntityIndex = childEntity->nextSiblingEntityIndex;
		}
		else
		{
			uint32 previousSiblingEntityIndex = previousParentEntity->firstChildEntityIndex;
			uint32 previousSiblingCount = 0;
			const uint32 maxPreviousSiblingCount = static_cast<uint32>(entityStorage.size());
			while (previousSiblingEntityIndex != invalidEntityIndex && previousSiblingCount < maxPreviousSiblingCount)
			{
				Entity* previousSiblingEntity = getEntity(previousSiblingEntityIndex);
				if (previousSiblingEntity == nullptr)
				{
					return false;
				}

				if (previousSiblingEntity->nextSiblingEntityIndex == childEntityIndex)
				{
					previousSiblingEntity->nextSiblingEntityIndex = childEntity->nextSiblingEntityIndex;
					break;
				}

				previousSiblingEntityIndex = previousSiblingEntity->nextSiblingEntityIndex;
				++previousSiblingCount;
			}

			if (previousSiblingEntityIndex == invalidEntityIndex || previousSiblingCount >= maxPreviousSiblingCount)
			{
				return false;
			}
		}
	}

	childEntity->parentEntityIndex = parentEntityIndex;
	childEntity->nextSiblingEntityIndex = invalidEntityIndex;
	if (parentEntity == nullptr)
	{
		return true;
	}

	if (parentEntity->firstChildEntityIndex == invalidEntityIndex)
	{
		parentEntity->firstChildEntityIndex = childEntityIndex;
		return true;
	}

	Entity* lastChildSiblingEntity = getEntity(lastChildSiblingEntityIndex);
	if (lastChildSiblingEntity == nullptr)
	{
		return false;
	}

	lastChildSiblingEntity->nextSiblingEntityIndex = childEntityIndex;
	return true;
}

bool World::attachComponent(const uint32 entityIndex, unique_pointer<Component> component)
{
	return attachComponent(entityIndex, moveValue(component), true);
}

bool World::attachComponent(
	const uint32 entityIndex,
	unique_pointer<Component> component,
	const bool initializeComponent)
{
	if (component == nullptr)
	{
		return false;
	}

	Entity* entity = getEntity(entityIndex);
	if (entity == nullptr)
	{
		return false;
	}

	componentStorage.push_back(moveValue(component));
	componentOwnerIndices.push_back(entityIndex);
	const uint32 componentIndex = static_cast<uint32>(componentStorage.size() - 1);
	entity->componentIndices.push_back(componentIndex);
	Component* createdComponent = componentStorage[componentIndex].get();
	if (createdComponent != nullptr)
	{
		createdComponent->setOwner(this, entityIndex, componentIndex, entity->getEntityHandle());
		if (initializeComponent)
		{
			createdComponent->initialize();
		}
	}
	return true;
}

void World::initializeRuntimeObjects()
{
	for (uint32 entityIndex = 0; entityIndex < static_cast<uint32>(entityStorage.size()); ++entityIndex)
	{
		Entity* entity = entityStorage[entityIndex].get();
		assert(entity != nullptr && "[World][Assert] reason=entity_missing");
		entity->initialize();
	}

	for (uint32 componentIndex = 0; componentIndex < static_cast<uint32>(componentStorage.size()); ++componentIndex)
	{
		Component* component = componentStorage[componentIndex].get();
		assert(component != nullptr && "[World][Assert] reason=component_missing");
		assert(componentIndex < static_cast<uint32>(componentOwnerIndices.size())
			&& "[World][Assert] reason=component_owner_index_missing");

		const uint32 ownerEntityIndex = componentOwnerIndices[componentIndex];
		Entity* ownerEntity = getEntity(ownerEntityIndex);
		assert(ownerEntity != nullptr && "[World][Assert] reason=component_owner_entity_missing");
		component->setOwner(this, ownerEntityIndex, componentIndex, ownerEntity->getEntityHandle());
		component->initialize();
	}
}

bool World::removeEntity(const uint32 entityIndex)
{
	Entity* entity = getEntity(entityIndex);
	if (entity == nullptr)
	{
		return false;
	}

	if (entity->parentEntityIndex != invalidEntityIndex)
	{
		Entity* parentEntity = getEntity(entity->parentEntityIndex);
		if (parentEntity != nullptr)
		{
			if (parentEntity->firstChildEntityIndex == entityIndex)
			{
				parentEntity->firstChildEntityIndex = entity->nextSiblingEntityIndex;
			}
			else
			{
				uint32 siblingEntityIndex = parentEntity->firstChildEntityIndex;
				uint32 siblingCount = 0;
				const uint32 maxSiblingCount = static_cast<uint32>(entityStorage.size());
				while (siblingEntityIndex != invalidEntityIndex && siblingCount < maxSiblingCount)
				{
					Entity* siblingEntity = getEntity(siblingEntityIndex);
					if (siblingEntity == nullptr)
					{
						break;
					}

					if (siblingEntity->nextSiblingEntityIndex == entityIndex)
					{
						siblingEntity->nextSiblingEntityIndex = entity->nextSiblingEntityIndex;
						break;
					}

					siblingEntityIndex = siblingEntity->nextSiblingEntityIndex;
					++siblingCount;
				}
			}
		}
	}

	uint32 childEntityIndex = entity->firstChildEntityIndex;
	uint32 childCount = 0;
	const uint32 maxChildCount = static_cast<uint32>(entityStorage.size());
	while (childEntityIndex != invalidEntityIndex && childCount < maxChildCount)
	{
		Entity* childEntity = getEntity(childEntityIndex);
		if (childEntity == nullptr)
		{
			break;
		}

		const uint32 nextChildEntityIndex = childEntity->nextSiblingEntityIndex;
		childEntity->parentEntityIndex = invalidEntityIndex;
		childEntity->nextSiblingEntityIndex = invalidEntityIndex;
		childEntityIndex = nextChildEntityIndex;
		++childCount;
	}

	while (!entity->componentIndices.empty())
	{
		const uint32 componentIndex = entity->componentIndices.back();
		if (!removeComponent(entityIndex, componentIndex))
		{
			entity->componentIndices.pop_back();
		}
	}

	const uint32 lastEntityIndex = static_cast<uint32>(entityStorage.size() - 1);
	if (entityIndex != lastEntityIndex)
	{
		entityStorage[entityIndex] = moveValue(entityStorage[lastEntityIndex]);

		Entity* movedEntity = entityStorage[entityIndex].get();
		if (movedEntity != nullptr)
		{
			movedEntity->ownerWorld = this;
			movedEntity->ownerEntityIndex = entityIndex;

			if (movedEntity->parentEntityIndex != invalidEntityIndex)
			{
				Entity* movedParentEntity = getEntity(movedEntity->parentEntityIndex);
				if (movedParentEntity != nullptr)
				{
					if (movedParentEntity->firstChildEntityIndex == lastEntityIndex)
					{
						movedParentEntity->firstChildEntityIndex = entityIndex;
					}
					else
					{
						uint32 movedSiblingEntityIndex = movedParentEntity->firstChildEntityIndex;
						uint32 movedSiblingCount = 0;
						const uint32 maxMovedSiblingCount = static_cast<uint32>(entityStorage.size());
						while (movedSiblingEntityIndex != invalidEntityIndex && movedSiblingCount < maxMovedSiblingCount)
						{
							Entity* movedSiblingEntity = getEntity(movedSiblingEntityIndex);
							if (movedSiblingEntity == nullptr)
							{
								break;
							}

							if (movedSiblingEntity->nextSiblingEntityIndex == lastEntityIndex)
							{
								movedSiblingEntity->nextSiblingEntityIndex = entityIndex;
								break;
							}

							movedSiblingEntityIndex = movedSiblingEntity->nextSiblingEntityIndex;
							++movedSiblingCount;
						}
					}
				}
			}

			uint32 movedChildEntityIndex = movedEntity->firstChildEntityIndex;
			uint32 movedChildCount = 0;
			const uint32 maxMovedChildCount = static_cast<uint32>(entityStorage.size());
			while (movedChildEntityIndex != invalidEntityIndex && movedChildCount < maxMovedChildCount)
			{
				Entity* movedChildEntity = getEntity(movedChildEntityIndex);
				if (movedChildEntity == nullptr)
				{
					break;
				}

				movedChildEntity->parentEntityIndex = entityIndex;
				movedChildEntityIndex = movedChildEntity->nextSiblingEntityIndex;
				++movedChildCount;
			}

			for (uint32 componentArrayIndex = 0; componentArrayIndex < static_cast<uint32>(movedEntity->componentIndices.size()); ++componentArrayIndex)
			{
				const uint32 movedComponentIndex = movedEntity->componentIndices[componentArrayIndex];
				if (movedComponentIndex < static_cast<uint32>(componentOwnerIndices.size()))
				{
					componentOwnerIndices[movedComponentIndex] = entityIndex;
				}

				Component* movedComponent = getComponentByIndex(movedComponentIndex);
				if (movedComponent != nullptr)
				{
					movedComponent->setOwner(this, entityIndex, movedComponentIndex, movedEntity->getEntityHandle());
					movedComponent->initialize();
				}
			}

			movedEntity->initialize();
		}
	}

	entityStorage.pop_back();
	return true;
}

bool World::removeComponent(const uint32 entityIndex, const uint32 componentIndex)
{
	Entity* entity = getEntity(entityIndex);
	if (entity == nullptr)
	{
		return false;
	}

	if (componentIndex >= static_cast<uint32>(componentStorage.size()))
	{
		return false;
	}

	if (componentStorage[componentIndex] == nullptr)
	{
		return false;
	}

	if (componentIndex >= static_cast<uint32>(componentOwnerIndices.size()))
	{
		return false;
	}

	if (componentOwnerIndices[componentIndex] != entityIndex)
	{
		return false;
	}

	if (!removeComponentIndexFromEntity(*entity, componentIndex))
	{
		return false;
	}

	const uint32 lastComponentIndex = static_cast<uint32>(componentStorage.size() - 1);
	if (componentIndex != lastComponentIndex)
	{
		componentStorage[componentIndex] = moveValue(componentStorage[lastComponentIndex]);

		const uint32 movedOwnerEntityIndex = componentOwnerIndices[lastComponentIndex];
		componentOwnerIndices[componentIndex] = movedOwnerEntityIndex;

		Entity* movedOwnerEntity = getEntity(movedOwnerEntityIndex);
		if (movedOwnerEntity != nullptr)
		{
			replaceComponentIndexInEntity(*movedOwnerEntity, lastComponentIndex, componentIndex);
		}

		Component* movedComponent = componentStorage[componentIndex].get();
		if (movedComponent != nullptr)
		{
			assert(movedOwnerEntity != nullptr);
			const BridgeHandle movedOwnerEntityHandle = movedOwnerEntity != nullptr
				? movedOwnerEntity->getEntityHandle()
				: invalidBridgeHandle;
			movedComponent->setOwner(this, movedOwnerEntityIndex, componentIndex, movedOwnerEntityHandle);
			movedComponent->initialize();
		}
	}

	componentStorage.pop_back();
	componentOwnerIndices.pop_back();
	return true;
}

void World::tick(const float deltaTimeSeconds)
{
	PROFILE_SCOPE("World", "World::tick");
	traversalEntityIndices.clear();
	traversalEntityIndices.reserve(entityStorage.size());

	for (uint32 entityIndex = 0; entityIndex < static_cast<uint32>(entityStorage.size()); ++entityIndex)
	{
		const Entity* entity = getEntity(entityIndex);
		if (entity == nullptr)
		{
			continue;
		}

		if (entity->parentEntityIndex == invalidEntityIndex)
		{
			traversalEntityIndices.push_back(entityIndex);
		}
	}

	for (uint32 traversalIndex = 0; traversalIndex < static_cast<uint32>(traversalEntityIndices.size()); ++traversalIndex)
	{
		const uint32 entityIndex = traversalEntityIndices[traversalIndex];
		Entity* entity = getEntity(entityIndex);
		if (entity == nullptr)
		{
			continue;
		}

		if (entity->active)
		{
			entity->tick(deltaTimeSeconds);
		}

		uint32 childEntityIndex = entity->firstChildEntityIndex;
		uint32 childCount = 0;
		const uint32 maxChildCount = static_cast<uint32>(entityStorage.size());
		while (childEntityIndex != invalidEntityIndex && childCount < maxChildCount)
		{
			traversalEntityIndices.push_back(childEntityIndex);

			const Entity* childEntity = getEntity(childEntityIndex);
			if (childEntity == nullptr)
			{
				break;
			}

			childEntityIndex = childEntity->nextSiblingEntityIndex;
			++childCount;
		}
	}
}

uint32 World::getEntityCount() const
{
	return static_cast<uint32>(entityStorage.size());
}

uint32 World::getComponentCount() const
{
	return static_cast<uint32>(componentStorage.size());
}

Entity* World::getEntityByIndex(const uint32 entityIndex)
{
	return getEntity(entityIndex);
}

const Entity* World::getEntityByIndex(const uint32 entityIndex) const
{
	return getEntity(entityIndex);
}

Component* World::getComponentByIndex(const uint32 componentIndex)
{
	if (componentIndex >= static_cast<uint32>(componentStorage.size()))
	{
		return nullptr;
	}

	return componentStorage[componentIndex].get();
}

const Component* World::getComponentByIndex(const uint32 componentIndex) const
{
	if (componentIndex >= static_cast<uint32>(componentStorage.size()))
	{
		return nullptr;
	}

	return componentStorage[componentIndex].get();
}

bool World::TEMP_tryGetEntityWorldTransform(const uint32 entityIndex, Transform& outTransform) const
{
	outTransform = {};

	const Entity* entity = getEntity(entityIndex);
	if (entity == nullptr || entity->getEntityType() != EntityType::placeableEntity)
	{
		return false;
	}

	float4x4 worldMatrix = {};
	if (!TEMP_buildEntityWorldMatrix(entityIndex, worldMatrix))
	{
		return false;
	}

	return TEMP_buildTransformFromMatrix4x4(worldMatrix, outTransform);
}

bool World::isValidEntityIndex(const uint32 entityIndex) const
{
	return entityIndex < static_cast<uint32>(entityStorage.size());
}

Entity* World::getEntity(const uint32 entityIndex)
{
	if (!isValidEntityIndex(entityIndex))
	{
		return nullptr;
	}

	return entityStorage[entityIndex].get();
}

const Entity* World::getEntity(const uint32 entityIndex) const
{
	if (!isValidEntityIndex(entityIndex))
	{
		return nullptr;
	}

	return entityStorage[entityIndex].get();
}

bool World::TEMP_buildEntityWorldMatrix(const uint32 entityIndex, float4x4& outWorldMatrix) const
{
	const Entity* entity = getEntity(entityIndex);
	if (entity == nullptr)
	{
		return false;
	}

	outWorldMatrix = TEMP_buildEntityLocalMatrix(*entity);
	const uint32 parentEntityIndex = entity->getParentEntityIndex();
	if (parentEntityIndex == invalidEntityIndex)
	{
		return true;
	}

	float4x4 parentWorldMatrix = {};
	if (!TEMP_buildEntityWorldMatrix(parentEntityIndex, parentWorldMatrix))
	{
		return false;
	}

	outWorldMatrix = multiplyMatrix4x4(outWorldMatrix, parentWorldMatrix);
	return true;
}

bool World::removeComponentIndexFromEntity(Entity& entity, const uint32 componentIndex)
{
	for (uint32 componentArrayIndex = 0; componentArrayIndex < static_cast<uint32>(entity.componentIndices.size()); ++componentArrayIndex)
	{
		if (entity.componentIndices[componentArrayIndex] != componentIndex)
		{
			continue;
		}

		const uint32 lastComponentArrayIndex = static_cast<uint32>(entity.componentIndices.size() - 1);
		entity.componentIndices[componentArrayIndex] = entity.componentIndices[lastComponentArrayIndex];
		entity.componentIndices.pop_back();
		return true;
	}

	return false;
}

bool World::replaceComponentIndexInEntity(
	Entity& entity,
	const uint32 fromComponentIndex,
	const uint32 toComponentIndex)
{
	for (uint32 componentArrayIndex = 0; componentArrayIndex < static_cast<uint32>(entity.componentIndices.size()); ++componentArrayIndex)
	{
		if (entity.componentIndices[componentArrayIndex] == fromComponentIndex)
		{
			entity.componentIndices[componentArrayIndex] = toComponentIndex;
			return true;
		}
	}

	return false;
}
