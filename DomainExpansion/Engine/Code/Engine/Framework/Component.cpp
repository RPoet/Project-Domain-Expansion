#include "Engine/Framework/Component.h"

#include "Engine/Common/XML/XML.h"
#include "Engine/Framework/Entity.h"
#include "Engine/Framework/World.h"

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
