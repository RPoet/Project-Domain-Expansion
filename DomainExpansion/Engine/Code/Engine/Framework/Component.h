#pragma once

#include "Bridge/BridgeHandle.h"
#include "Engine/Assets/Asset.h"
#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Platform/PlatformDefine.h"

class World;

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
	} \
	DECLARE_ASSET(componentClassName);

class Component : public Asset
{
public:
	DECLARE_ASSET(Component);
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
	void clear() override;

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

	const string& getOwnerEntityAssetPath() const
	{
		return ownerEntityAssetPath;
	}

	void setOwnerEntityAssetPath(const string& ownerEntityAssetPath)
	{
		this->ownerEntityAssetPath = ownerEntityAssetPath;
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

	void writeAssetProperty(OutputFileStream& fileStream) const override;
	void readAssetProperty(const XMLKeyValueDocument& document) override;

	string ownerEntityAssetPath = {};
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
