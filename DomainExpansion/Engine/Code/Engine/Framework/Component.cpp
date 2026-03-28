#include "Engine/Framework/Component.h"

#include "Engine/Framework/Entity.h"
#include "Engine/Framework/World.h"

static unordered_map<string, ComponentFactoryFunction>& getComponentFactoryByTypeName()
{
	static unordered_map<string, ComponentFactoryFunction> componentFactoryByTypeName = {};
	return componentFactoryByTypeName;
}

ComponentFactoryRegistration::ComponentFactoryRegistration(const char* assetTypeName, const ComponentFactoryFunction createFactory)
{
	assert(assetTypeName != nullptr && "[Component][Assert] reason=component_factory_asset_type_name_missing");
	assert(createFactory != nullptr && "[Component][Assert] reason=component_factory_create_function_missing");
	getComponentFactoryByTypeName().emplace(assetTypeName, createFactory);
}

unique_pointer<Component> Component::createByAssetTypeName(const string& assetTypeName)
{
	const auto foundFactory = getComponentFactoryByTypeName().find(assetTypeName);
	assert(foundFactory != getComponentFactoryByTypeName().end() && "[Component][Assert] reason=component_factory_missing");
	return foundFactory->second();
}

void Component::clear()
{
	Asset::clear();
	ownerEntityAssetPath.clear();
}

void Component::writeAssetProperty(OutputFileStream& fileStream) const
{
	XML& xml = XML::get();
	string serializedOwnerEntityAssetPath = ownerEntityAssetPath;
	if (ownerWorld != nullptr && ownerEntityIndex != invalidEntityIndex)
	{
		const Entity* ownerEntity = ownerWorld->getEntityByIndex(ownerEntityIndex);
		assert(ownerEntity != nullptr && "[Component][Assert] reason=owner_entity_missing");
		assert(!ownerEntity->getAssetPath().empty() && "[Component][Assert] reason=owner_entity_asset_path_missing");
		serializedOwnerEntityAssetPath = ownerEntity->getAssetPath();
	}

	xml.writeProperty(fileStream, "ownerEntityAssetPath", serializedOwnerEntityAssetPath);
}

void Component::readAssetProperty(const XMLKeyValueDocument& document)
{
	XML& xml = XML::get();
	xml.readProperty(document, "deasset.ownerEntityAssetPath", ownerEntityAssetPath);
}
