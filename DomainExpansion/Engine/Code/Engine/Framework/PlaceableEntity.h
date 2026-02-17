#pragma once

#include "Engine/Framework/Entity.h"
#include "Engine/Framework/Transform.h"

class PlaceableEntity : public Entity
{
public:
	Transform transform;

private:
	friend class World;
	explicit PlaceableEntity(memory_resource* componentIndexMemoryResource = nullptr);
};
