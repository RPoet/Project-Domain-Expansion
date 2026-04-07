#include "Engine/Assets/AssetLoader.h"

#include "Engine/Framework/World.h"

void AssetLoader::saveWorld(World& world) const
{
	assert(!world.getAssetPath().empty() && "[AssetLoader][Assert] reason=world_asset_path_missing");
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();

	for (uint32 entityIndex = 0; entityIndex < world.getEntityCount(); ++entityIndex)
	{
		Entity* entity = world.getEntityByIndex(entityIndex);
		assert(entity != nullptr && "[AssetLoader][Assert] reason=entity_missing");
		if (!diskLoaderModule->isWorldOwnedAssetPath(world.getAssetPath(), entity->getAssetPath()))
		{
			entity->setAssetPath(diskLoaderModule->buildGeneratedWorldEntityAssetPath(world.getAssetPath(), entityIndex));
		}
	}

	for (uint32 entityIndex = 0; entityIndex < world.getEntityCount(); ++entityIndex)
	{
		Entity* entity = world.getEntityByIndex(entityIndex);
		assert(entity != nullptr && "[AssetLoader][Assert] reason=entity_missing");
		for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
		{
			const uint32 componentIndex = entity->getComponentIndex(componentArrayIndex);
			Component* component = world.getComponentByIndex(componentIndex);
			assert(component != nullptr && "[AssetLoader][Assert] reason=component_missing");
			if (!diskLoaderModule->isWorldOwnedAssetPath(world.getAssetPath(), component->getAssetPath()))
			{
				component->setAssetPath(diskLoaderModule->buildGeneratedWorldComponentAssetPath(world.getAssetPath(), componentIndex));
			}
		}
	}

	for (uint32 entityIndex = 0; entityIndex < world.getEntityCount(); ++entityIndex)
	{
		Entity* entity = world.getEntityByIndex(entityIndex);
		assert(entity != nullptr && "[AssetLoader][Assert] reason=entity_missing");
		for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
		{
			const uint32 componentIndex = entity->getComponentIndex(componentArrayIndex);
			Component* component = world.getComponentByIndex(componentIndex);
			assert(component != nullptr && "[AssetLoader][Assert] reason=component_missing");
			component->setOwnerEntityAssetPath(entity->getAssetPath());
			assert(!component->getAssetPath().empty() && "[AssetLoader][Assert] reason=component_asset_path_missing");
			OutputFileStream componentFileStream = diskLoaderModule->openOutputFileStream(diskLoaderModule->resolveAbsolutePathFromResources(component->getAssetPath()), false, true);
			component->writeProperty(componentFileStream);
		}

		assert(!entity->getAssetPath().empty() && "[AssetLoader][Assert] reason=entity_asset_path_missing");
		OutputFileStream entityFileStream = diskLoaderModule->openOutputFileStream(diskLoaderModule->resolveAbsolutePathFromResources(entity->getAssetPath()), false, true);
		entity->writeProperty(entityFileStream);
	}

	OutputFileStream worldFileStream = diskLoaderModule->openOutputFileStream(diskLoaderModule->resolveAbsolutePathFromResources(world.getAssetPath()), false, true);
	world.writeProperty(worldFileStream);
}

template <>
unique_pointer<World> AssetLoader::loadUniqueAsset<World>(const string& assetPathReference) const
{
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	const string worldAssetPath = diskLoaderModule->resolveAssetPath(assetPathReference, DiskLoaderModule::AssetFileType::document);
	PROFILE_SCOPE_DETAIL("startup", "AssetLoader::loadWorld", worldAssetPath);
	const XMLKeyValueDocument worldDocument = XML::get().readDocumentFile(worldAssetPath);

	unique_pointer<World> loadedWorld(new World());
	loadedWorld->setAssetPath(worldAssetPath);
	loadedWorld->readProperty(worldDocument);

	unordered_map<string, uint32> entityIndexByAssetPath = {};
	vector<string> assetPaths = {};
	XML::get().readPropertyArray(worldDocument, "deasset.Entities", assetPaths);

	{
		PROFILE_SCOPE("startup", "AssetLoader::loadWorld.entities");
		for (uint32 entityReferenceIndex = 0; entityReferenceIndex < static_cast<uint32>(assetPaths.size()); ++entityReferenceIndex)
		{
			const string entityAssetPath = diskLoaderModule->resolveAssetPath(assetPaths[entityReferenceIndex], DiskLoaderModule::AssetFileType::document);
			const string absoluteEntityAssetPath = diskLoaderModule->resolveAbsolutePathFromResources(entityAssetPath);
			const XMLKeyValueDocument entityDocument = XML::get().readDocumentFile(entityAssetPath);
			const string* entityTypeName = entityDocument.find("deasset.@type");
			assert(entityTypeName != nullptr && "[AssetLoader][Assert] reason=entity_document_type_missing");

			unique_pointer<Entity> entityObject = Entity::createByAssetTypeName(*entityTypeName);
			assert(entityObject != nullptr && "[AssetLoader][Assert] reason=entity_create_failed");

			const uint32 entityIndex = loadedWorld->addEntityObject(moveValue(entityObject), false);
			Entity* entity = loadedWorld->getEntityByIndex(entityIndex);
			assert(entity != nullptr && "[AssetLoader][Assert] reason=entity_missing");

			entity->setAssetPath(entityAssetPath);
			entity->readProperty(entityDocument);
			entityIndexByAssetPath[absoluteEntityAssetPath] = entityIndex;
		}
	}

	{
		PROFILE_SCOPE("startup", "AssetLoader::loadWorld.hierarchy");
		for (uint32 entityIndex = 0; entityIndex < loadedWorld->getEntityCount(); ++entityIndex)
		{
			Entity* entity = loadedWorld->getEntityByIndex(entityIndex);
			assert(entity != nullptr && "[AssetLoader][Assert] reason=entity_missing");
			if (entity->getParentEntityAssetPath().empty())
			{
				continue;
			}

			const string absoluteParentEntityAssetPath = diskLoaderModule->resolveAbsolutePathFromResources(entity->getParentEntityAssetPath());
			const auto parentEntityIterator = entityIndexByAssetPath.find(absoluteParentEntityAssetPath);
			assert(parentEntityIterator != entityIndexByAssetPath.end() && "[AssetLoader][Assert] reason=parent_entity_asset_reference_missing");

			const bool addedChildEntity = loadedWorld->addChildEntity(parentEntityIterator->second, entityIndex);
			assert(addedChildEntity && "[AssetLoader][Assert] reason=child_entity_attach_failed");
		}
	}

	XML::get().readPropertyArray(worldDocument, "deasset.Components", assetPaths);

	{
		PROFILE_SCOPE("startup", "AssetLoader::loadWorld.components");
		for (uint32 componentReferenceIndex = 0; componentReferenceIndex < static_cast<uint32>(assetPaths.size()); ++componentReferenceIndex)
		{
			const string componentAssetPath = diskLoaderModule->resolveAssetPath(assetPaths[componentReferenceIndex], DiskLoaderModule::AssetFileType::document);
			const XMLKeyValueDocument componentDocument = XML::get().readDocumentFile(componentAssetPath);
			const string* componentTypeName = componentDocument.find("deasset.@type");
			assert(componentTypeName != nullptr && "[AssetLoader][Assert] reason=component_document_type_missing");

			unique_pointer<Component> component = Component::createByAssetTypeName(*componentTypeName);
			assert(component != nullptr && "[AssetLoader][Assert] reason=component_create_failed");

			component->setAssetPath(componentAssetPath);
			component->readProperty(componentDocument);

			const string absoluteOwnerEntityAssetPath = diskLoaderModule->resolveAbsolutePathFromResources(component->getOwnerEntityAssetPath());
			const auto ownerEntityIterator = entityIndexByAssetPath.find(absoluteOwnerEntityAssetPath);
			assert(ownerEntityIterator != entityIndexByAssetPath.end() && "[AssetLoader][Assert] reason=component_owner_entity_asset_reference_missing");

			const bool attachedComponent = loadedWorld->attachComponent(ownerEntityIterator->second, moveValue(component), false);
			assert(attachedComponent && "[AssetLoader][Assert] reason=component_attach_failed");
		}
	}

	{
		PROFILE_SCOPE("startup", "AssetLoader::loadWorld.runtime_objects");
		loadedWorld->initializeRuntimeObjects();
	}

	return loadedWorld;
}
