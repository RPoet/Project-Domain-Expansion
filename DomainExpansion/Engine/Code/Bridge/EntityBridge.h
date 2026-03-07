#pragma once

#include "Bridge/BridgeHandle.h"
#include "Bridge/DefaultBridge.h"
#include "Engine/Framework/EntityObject.h"
#include "Engine/Framework/Transform.h"

struct EntityBridgeObject
{
	using StaticProperty = EntityObject::StaticProperty;

	struct DynamicProperty
	{
		uint32 entityIndex = invalidEntityIndex;
		bool hasTransform = false;
		Transform transform = {};
	};

	struct ObjectDesc
	{
		StaticProperty staticProperty = {};
		DynamicProperty dynamicProperty = {};
	};
};

class EntityBridge final : public DefaultBridge<EntityBridgeObject>
{
public:
	EntityBridge(const EntityBridge&) = delete;
	EntityBridge& operator=(const EntityBridge&) = delete;
	EntityBridge(EntityBridge&&) = delete;
	EntityBridge& operator=(EntityBridge&&) = delete;

	static EntityBridge& get()
	{
		static EntityBridge entityBridge = {};
		return entityBridge;
	}

	HandleReference createEntityHandle(const ObjectDesc& objectDesc);
	bool isHandleAlive(const HandleReference& handleReference) const;
	bool isHandleAlive(PackedHandle packedHandle) const;

	const StaticData* getStaticData(PackedHandle packedHandle) const;
	const StaticData* getStaticData(const HandleReference& handleReference) const;
	const DynamicData* getDynamicData(PackedHandle packedHandle) const;
	const DynamicData* getDynamicData(const HandleReference& handleReference) const;
	void updateDynamicData(PackedHandle packedHandle, const DynamicData& dynamicData);
	void updateDynamicData(const HandleReference& handleReference, const DynamicData& dynamicData);

	void processFrame();

private:
	EntityBridge() = default;
	~EntityBridge() = default;
};
