#pragma once

#include "Bridge/BridgeHandle.h"
#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Platform/PlatformDefine.h"

class World;
class Component;

struct ComponentType
{
	const void* key = nullptr;

	constexpr bool isValid() const
	{
		return key != nullptr;
	}

	constexpr bool operator==(const ComponentType& other) const = default;
};

struct ComponentTypeMetadata
{
	ComponentType type = {};
	const char* classNameText = "Component";
};

#define DECLARE_COMPONENT(componentClassName) \
public: \
	inline static const int componentTypeTagValue = 0; \
	static constexpr ComponentType staticComponentType = {&componentTypeTagValue}; \
	static const ComponentTypeMetadata& getStaticComponentTypeMetadata() \
	{ \
		static const ComponentTypeMetadata componentTypeMetadata = {staticComponentType, #componentClassName}; \
		return componentTypeMetadata; \
	} \
	ComponentType getComponentType() const override \
	{ \
		return staticComponentType; \
	} \
	const ComponentTypeMetadata& getComponentTypeMetadata() const override \
	{ \
		return getStaticComponentTypeMetadata(); \
	}

class Component
{
public:
	virtual ~Component() = default;
	static constexpr ComponentType staticComponentType = {};
	static const ComponentTypeMetadata& getStaticComponentTypeMetadata()
	{
		static const ComponentTypeMetadata componentTypeMetadata = {};
		return componentTypeMetadata;
	}

	virtual ComponentType getComponentType() const
	{
		return staticComponentType;
	}

	virtual const ComponentTypeMetadata& getComponentTypeMetadata() const
	{
		return getStaticComponentTypeMetadata();
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

template<typename ComponentClass>
bool isComponentType(const Component* component)
{
	return component != nullptr && component->getComponentType() == ComponentClass::staticComponentType;
}

template<typename ComponentClass>
ComponentClass* componentCast(Component* component)
{
	return isComponentType<ComponentClass>(component)
		? static_cast<ComponentClass*>(component)
		: nullptr;
}

template<typename ComponentClass>
const ComponentClass* componentCast(const Component* component)
{
	return isComponentType<ComponentClass>(component)
		? static_cast<const ComponentClass*>(component)
		: nullptr;
}
