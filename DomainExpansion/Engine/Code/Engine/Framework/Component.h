#pragma once

#include "Bridge/BridgeHandle.h"
#include "Engine/Assets/Asset.h"
#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Platform/PlatformDefine.h"

class World;
class Component;

using ComponentFactoryFunction = unique_pointer<Component>(*)();

struct ComponentFactoryRegistration
{
	ComponentFactoryRegistration(const char* assetTypeName, ComponentFactoryFunction createFactory);
};

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
	DECLARE_ASSET(componentClassName); \
	static unique_pointer<Component> createFactoryInstance() { return unique_pointer<Component>(new componentClassName()); } \
	inline static uint32 componentTypeTagValue = 0; \
	inline static const ComponentFactoryRegistration componentFactoryRegistration = {componentClassName::getStaticAssetTypeName(), &componentClassName::createFactoryInstance}; \
	inline static const ComponentType staticComponentType = {&componentTypeTagValue}; \
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

class Component : public Asset
{
public:
	DECLARE_ASSET(Component);
	virtual ~Component() = default;
	static unique_pointer<Component> createByAssetTypeName(const string& assetTypeName);
	static constexpr ComponentType staticComponentType = {};
	inline static const ComponentTypeMetadata staticComponentTypeMetadata = {};
	static const ComponentTypeMetadata& getStaticComponentTypeMetadata()
	{
		return staticComponentTypeMetadata;
	}

	virtual ComponentType getComponentType() const
	{
		return staticComponentType;
	}

	virtual const ComponentTypeMetadata& getComponentTypeMetadata() const
	{
		return staticComponentTypeMetadata;
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

	virtual void initialize();

	virtual void tick(float deltaTimeSeconds) { static_cast<void>(deltaTimeSeconds); }

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

	void writeAssetProperty(OutputFileStream& fileStream) const override;
	void readAssetProperty(const XMLKeyValueDocument& document) override;

	DECLARE_FIELD(string, ownerEntityAssetPath, "");
	World* ownerWorld = nullptr;
	uint32 ownerEntityIndex = invalidEntityIndex;
	uint32 ownerComponentIndex = invalidComponentIndex;
	BridgeHandle ownerEntityHandle = invalidBridgeHandle;
};

template<typename ComponentClass>
ComponentClass* componentCast(Component* component)
{
	if (component == nullptr || component->getComponentType() != ComponentClass::staticComponentType)
	{
		return nullptr;
	}

	return static_cast<ComponentClass*>(component);
}

template<typename ComponentClass>
const ComponentClass* componentCast(const Component* component)
{
	if (component == nullptr || component->getComponentType() != ComponentClass::staticComponentType)
	{
		return nullptr;
	}

	return static_cast<const ComponentClass*>(component);
}
