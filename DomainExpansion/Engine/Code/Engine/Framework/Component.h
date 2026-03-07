#pragma once

#include "Bridge/BridgeHandle.h"
#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Platform/PlatformDefine.h"

class World;

enum class ComponentType : uint32
{
	component = 0,
	meshComponent = 1,
};

class Component
{
public:
	virtual ~Component() = default;
	virtual ComponentType getComponentType() const
	{
		return ComponentType::component;
	}

	World* getOwnerWorld()
	{
		return ownerWorld;
	}

	const World* getOwnerWorld() const
	{
		return ownerWorld;
	}

	uint32 getOwnerEntityIndex() const
	{
		return ownerEntityIndex;
	}

	uint32 getComponentIndex() const
	{
		return ownerComponentIndex;
	}

	BridgeHandle getOwnerEntityHandle() const
	{
		return ownerEntityHandle;
	}

	virtual void tick(float deltaTimeSeconds)
	{
		unused(deltaTimeSeconds);
	}

protected:
	friend class World;
	void setOwner(
		World* ownerWorld,
		const uint32 ownerEntityIndex,
		const uint32 ownerComponentIndex,
		const BridgeHandle ownerEntityHandle)
	{
		this->ownerWorld = ownerWorld;
		this->ownerEntityIndex = ownerEntityIndex;
		this->ownerComponentIndex = ownerComponentIndex;
		this->ownerEntityHandle = ownerEntityHandle;
	}

	virtual void initComponent()
	{
	}

	World* ownerWorld = nullptr;
	uint32 ownerEntityIndex = invalidEntityIndex;
	uint32 ownerComponentIndex = invalidComponentIndex;
	BridgeHandle ownerEntityHandle = invalidBridgeHandle;
};
