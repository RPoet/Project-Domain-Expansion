#pragma once

#include "Bridge/EntityBridge.h"
#include "Engine/Framework/EntityObject.h"
#include "Engine/Framework/FrameworkConstants.h"

class Component;
class World;

enum class EntityType : uint32
{
	entity = 0,
	placeableEntity = 1,
};

class Entity
{
public:
	virtual ~Entity() = default;
	virtual EntityType getEntityType() const
	{
		return EntityType::entity;
	}
	virtual void tick(float deltaTimeSeconds);

	bool addComponent(unique_pointer<Component> component);
	bool removeComponent(uint32 componentIndex);
	World* getOwnerWorld();
	const World* getOwnerWorld() const;
	uint32 getEntityIndex() const;
	BridgeHandle getEntityHandle() const;
	uint32 getComponentCount() const;
	uint32 getComponentIndex(uint32 componentArrayIndex) const;
	void setActive(bool active);

	pooled_vector<uint32> componentIndices;
	uint32 parentEntityIndex = invalidEntityIndex;
	uint32 firstChildEntityIndex = invalidEntityIndex;
	uint32 nextSiblingEntityIndex = invalidEntityIndex;
	bool active = true;

protected:
	friend class World;
	explicit Entity(memory_resource* componentIndexMemoryResource = nullptr);
	virtual void initEntity();
	virtual void buildEntityBridgeDynamicData(EntityBridge::DynamicData& dynamicData);
	void requestEntityBridgeUpdate();
	void tickComponents(float deltaTimeSeconds);

	World* ownerWorld = nullptr;
	uint32 ownerEntityIndex = invalidEntityIndex;
	EntityBridge::HandleReference entityBridgeHandle = {};
};
