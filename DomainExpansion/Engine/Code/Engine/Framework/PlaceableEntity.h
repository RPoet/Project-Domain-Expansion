#pragma once

#include "Engine/Framework/Entity.h"
#include "Engine/Framework/Transform.h"

class PlaceableEntity : public Entity
{
public:
	DECLARE_ASSET(PlaceableEntity);
	void clear() override;

	EntityType getEntityType() const override
	{
		return EntityType::placeableEntity;
	}
	Transform transform;

private:
	friend class World;
	explicit PlaceableEntity(memory_resource* componentIndexMemoryResource = nullptr);
	void buildEntityBridgeDynamicData(EntityBridge::DynamicData& dynamicData) override;
	void writeAssetProperty(OutputFileStream& fileStream) const override;
	void readAssetProperty(const XMLKeyValueDocument& document) override;
};
