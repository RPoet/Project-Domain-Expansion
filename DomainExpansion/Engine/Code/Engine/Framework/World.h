#pragma once

#include "Engine/Assets/Asset.h"
#include "Engine/Framework/Component.h"
#include "Engine/Framework/Entity.h"
#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Framework/PlaceableEntity.h"

class AssetLoader;

class World : public Asset
{
public:
	DECLARE_ASSET(World);
	explicit World(const string& worldName = "");
	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World(World&&) = delete;
	World& operator=(World&&) = delete;
	~World() = default;

	void clear() override;

	uint32 createEntity();
	uint32 createPlaceableEntity();
	bool addChildEntity(uint32 parentEntityIndex, uint32 childEntityIndex);
	bool reparentEntity(uint32 childEntityIndex, uint32 parentEntityIndex);
	bool attachComponent(uint32 entityIndex, unique_pointer<Component> component);
	bool removeEntity(uint32 entityIndex);
	bool removeComponent(uint32 entityIndex, uint32 componentIndex);
	void tick(float deltaTimeSeconds);

	uint32 getEntityCount() const;
	uint32 getComponentCount() const;
	Entity* getEntityByIndex(uint32 entityIndex);
	const Entity* getEntityByIndex(uint32 entityIndex) const;
	Component* getComponentByIndex(uint32 componentIndex);
	const Component* getComponentByIndex(uint32 componentIndex) const;
	// TODO: Remove this TEMP_ query path after cached world transforms live on Entity and update in World::tick().
	bool TEMP_tryGetEntityWorldTransform(uint32 entityIndex, Transform& outTransform) const;

private:
	friend class AssetLoader;
	void initializeRuntimeObjects();
	void writeAssetProperty(OutputFileStream& fileStream) const override;
	void readAssetProperty(const XMLKeyValueDocument& document) override;
	uint32 createEntity(bool initializeEntity);
	uint32 createPlaceableEntity(bool initializeEntity);
	bool attachComponent(uint32 entityIndex, unique_pointer<Component> component, bool initializeComponent);

	uint32 addEntityObject(unique_pointer<Entity> entity, bool initializeEntity);
	bool isValidEntityIndex(uint32 entityIndex) const;
	Entity* getEntity(uint32 entityIndex);
	const Entity* getEntity(uint32 entityIndex) const;
	bool TEMP_buildEntityWorldMatrix(uint32 entityIndex, float4x4& outWorldMatrix) const;
	bool removeComponentIndexFromEntity(Entity& entity, uint32 componentIndex);
	bool replaceComponentIndexInEntity(Entity& entity, uint32 fromComponentIndex, uint32 toComponentIndex);

	unsynchronized_pool_resource componentIndexPoolResource;
	vector<unique_pointer<Component>> componentStorage;
	vector<uint32> componentOwnerIndices;
	vector<unique_pointer<Entity>> entityStorage;
	vector<uint32> traversalEntityIndices;
};
