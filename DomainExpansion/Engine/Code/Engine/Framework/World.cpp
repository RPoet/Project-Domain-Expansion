#include "Engine/Framework/World.h"

World::World(const wstring& worldName)
	: worldName(worldName)
{
}

const wstring& World::getWorldName() const
{
	return worldName;
}

void World::setWorldName(const wstring& worldName)
{
	this->worldName = worldName;
}

uint32 World::createEntity()
{
	unique_pointer<Entity> entity(new Entity(&componentIndexPoolResource));
	entityStorage.push_back(moveValue(entity));
	const uint32 entityIndex = static_cast<uint32>(entityStorage.size() - 1);
	Entity* createdEntity = entityStorage[entityIndex].get();
	if (createdEntity != nullptr)
	{
		assignEntityOwnership(*createdEntity, entityIndex);
	}

	return entityIndex;
}

uint32 World::createPlaceableEntity()
{
	unique_pointer<Entity> placeableEntity(new PlaceableEntity(&componentIndexPoolResource));
	entityStorage.push_back(moveValue(placeableEntity));
	const uint32 entityIndex = static_cast<uint32>(entityStorage.size() - 1);
	Entity* createdEntity = entityStorage[entityIndex].get();
	if (createdEntity != nullptr)
	{
		assignEntityOwnership(*createdEntity, entityIndex);
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

	childEntity->parentEntityIndex = parentEntityIndex;
	childEntity->nextSiblingEntityIndex = invalidEntityIndex;

	if (parentEntity->firstChildEntityIndex == invalidEntityIndex)
	{
		parentEntity->firstChildEntityIndex = childEntityIndex;
		return true;
	}

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

		if (siblingEntity->nextSiblingEntityIndex == invalidEntityIndex)
		{
			siblingEntity->nextSiblingEntityIndex = childEntityIndex;
			return true;
		}

		siblingEntityIndex = siblingEntity->nextSiblingEntityIndex;
		++siblingCount;
	}

	return false;
}

bool World::attachComponent(const uint32 entityIndex, unique_pointer<Component> component)
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
	return true;
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
			assignEntityOwnership(*movedEntity, entityIndex);

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
			}
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
	}

	componentStorage.pop_back();
	componentOwnerIndices.pop_back();
	return true;
}

bool World::tick(const float deltaTimeSeconds)
{
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
		const Entity* entity = getEntity(entityIndex);
		if (entity == nullptr)
		{
			continue;
		}

		if (entity->activeState)
		{
			tickEntityComponents(*entity, deltaTimeSeconds);
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

	return true;
}

void World::clear()
{
	componentStorage.clear();
	componentOwnerIndices.clear();
	entityStorage.clear();
	traversalEntityIndices.clear();
	componentIndexPoolResource.release();
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

void World::tickEntityComponents(const Entity& entity, const float deltaTimeSeconds)
{
	for (uint32 componentArrayIndex = 0; componentArrayIndex < static_cast<uint32>(entity.componentIndices.size()); ++componentArrayIndex)
	{
		const uint32 componentIndex = entity.componentIndices[componentArrayIndex];
		if (componentIndex >= static_cast<uint32>(componentStorage.size()))
		{
			continue;
		}

		if (componentStorage[componentIndex] == nullptr)
		{
			continue;
		}

		componentStorage[componentIndex]->tick(deltaTimeSeconds);
	}
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

void World::assignEntityOwnership(Entity& entity, const uint32 entityIndex)
{
	entity.ownerWorld = this;
	entity.ownerEntityIndex = entityIndex;
}
