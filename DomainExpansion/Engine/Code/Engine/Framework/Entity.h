#pragma once

#include "Engine/Framework/FrameworkConstants.h"

class Component;
class World;

class Entity
{
public:
	virtual ~Entity() = default;

	bool addComponent(unique_pointer<Component> component);
	bool removeComponent(uint32 componentIndex);
	World* getOwnerWorld();
	const World* getOwnerWorld() const;
	uint32 getEntityIndex() const;
	uint32 getComponentCount() const;
	uint32 getComponentIndex(uint32 componentArrayIndex) const;

	pooled_vector<uint32> componentIndices;
	uint32 parentEntityIndex = invalidEntityIndex;
	uint32 firstChildEntityIndex = invalidEntityIndex;
	uint32 nextSiblingEntityIndex = invalidEntityIndex;
	bool activeState = true;

protected:
	friend class World;
	explicit Entity(memory_resource* componentIndexMemoryResource = nullptr);

	World* ownerWorld = nullptr;
	uint32 ownerEntityIndex = invalidEntityIndex;
};
