#pragma once

#include "Engine/Framework/Component.h"
#include "Engine/Framework/Entity.h"
#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Framework/PlaceableEntity.h"

class World
{
public:
	explicit World(const wstring& worldName = L"");
	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World(World&&) = delete;
	World& operator=(World&&) = delete;
	~World() = default;

	const wstring& getWorldName() const;
	void setWorldName(const wstring& worldName);

	uint32 createEntity();
	uint32 createPlaceableEntity();
	bool addChildEntity(uint32 parentEntityIndex, uint32 childEntityIndex);
	bool attachComponent(uint32 entityIndex, unique_pointer<Component> component);
	bool removeEntity(uint32 entityIndex);
	bool removeComponent(uint32 entityIndex, uint32 componentIndex);
	void tick(float deltaTimeSeconds);
	void clear();

	uint32 getEntityCount() const;
	uint32 getComponentCount() const;
	Entity* getEntityByIndex(uint32 entityIndex);
	const Entity* getEntityByIndex(uint32 entityIndex) const;
	Component* getComponentByIndex(uint32 componentIndex);
	const Component* getComponentByIndex(uint32 componentIndex) const;

private:
	uint32 addEntityObject(unique_pointer<Entity> entity);
	bool isValidEntityIndex(uint32 entityIndex) const;
	Entity* getEntity(uint32 entityIndex);
	const Entity* getEntity(uint32 entityIndex) const;
	bool removeComponentIndexFromEntity(Entity& entity, uint32 componentIndex);
	bool replaceComponentIndexInEntity(Entity& entity, uint32 fromComponentIndex, uint32 toComponentIndex);

	wstring worldName;
	unsynchronized_pool_resource componentIndexPoolResource;
	vector<unique_pointer<Component>> componentStorage;
	vector<uint32> componentOwnerIndices;
	vector<unique_pointer<Entity>> entityStorage;
	vector<uint32> traversalEntityIndices;
};
