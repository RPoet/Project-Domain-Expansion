#include "Engine/Framework/Entity.h"
#include "Engine/Framework/Component.h"
#include "Engine/Framework/World.h"

Entity::Entity(memory_resource* componentIndexMemoryResource)
	: componentIndices(componentIndexMemoryResource != nullptr ? componentIndexMemoryResource : getDefaultMemoryResource())
{
}

bool Entity::addComponent(unique_pointer<Component> component)
{
	if (ownerWorld == nullptr)
	{
		return false;
	}

	return ownerWorld->attachComponent(ownerEntityIndex, moveValue(component));
}

bool Entity::removeComponent(const uint32 componentIndex)
{
	if (ownerWorld == nullptr)
	{
		return false;
	}

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
