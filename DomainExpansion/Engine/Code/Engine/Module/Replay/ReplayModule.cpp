#include "Engine/Module/Replay/ReplayModule.h"

#include "Engine/Assets/AssetLoader.h"
#include "Engine/Common/EditorCommandCommon.h"
#include "Engine/Common/XML/XML.h"
#include "Engine/Framework/CameraComponent.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/World.h"
#include "Engine/Module/CLI/CLIModule.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/MeshStreaming/MeshStreaming.h"

static Framework* getReplayFramework()
{
	shared_pointer<ReplayModule> replayModule = ReplayModule::get();
	assert(replayModule != nullptr && "[ReplayModule][Assert] reason=module_missing");
	return replayModule->getFrameworkReference();
}

static World* requireReplayActiveWorld()
{
	Framework* framework = getReplayFramework();
	return framework != nullptr ? framework->getActiveWorld() : nullptr;
}

static bool saveReplayMaterialAssetDocument(const MaterialAsset& materialAsset)
{
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	if (diskLoaderModule == nullptr)
	{
		return false;
	}

	string absoluteAssetPath = {};
	if (!diskLoaderModule->resolveAbsolutePathFromResources(materialAsset.getAssetPath(), absoluteAssetPath))
	{
		return false;
	}

	OutputFileStream materialAssetFileStream = {};
	if (!diskLoaderModule->openOutputFileStream(absoluteAssetPath, materialAssetFileStream, false, true))
	{
		return false;
	}

	materialAsset.writeProperty(materialAssetFileStream);
	return materialAssetFileStream.good();
}

bool ReplayModule::init(Framework& framework)
{
	frameworkReference = &framework;
	return true;
}

void ReplayModule::preUpdate()
{
}

void ReplayModule::postUpdate()
{
}

void ReplayModule::shutdown()
{
	frameworkReference = nullptr;
}

Framework* ReplayModule::getFrameworkReference() const
{
	return frameworkReference;
}

void ReplayModule::registerReplayCommands()
{
	const bool saveActiveWorldRegistered = CLIModule::registerCommand(
		"Framework.saveActiveWorld",
		[](const vector<string>& arguments)
		{
			unused(arguments);
			Framework* framework = getReplayFramework();
			if (framework == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			return framework->saveActiveWorld()
				? static_cast<int32>(CLIModule::ExecutionCode::succeeded)
				: static_cast<int32>(FrameworkExecutionCode::saveActiveWorldFailed);
		});
	assert(saveActiveWorldRegistered && "[ReplayModule][Assert] reason=save_active_world_command_register_failed");
	unused(saveActiveWorldRegistered);

	const bool loadWorldRegistered = CLIModule::registerCommand(
		"Editor.loadWorld",
		[](const vector<string>& arguments)
		{
			Framework* framework = getReplayFramework();
			if (framework == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			if (arguments.size() != 1)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			World* loadedWorld = framework->loadWorld(arguments[0]);
			if (loadedWorld == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::worldLoadFailed);
			}

			return editorCommandAreEquivalentAssetPaths(loadedWorld->getAssetPath(), arguments[0])
				? static_cast<int32>(CLIModule::ExecutionCode::succeeded)
				: static_cast<int32>(EditorExecutionCode::assetPathMismatch);
		});
	assert(loadWorldRegistered && "[ReplayModule][Assert] reason=editor_load_world_command_register_failed");
	unused(loadWorldRegistered);

	const bool createWorldRegistered = CLIModule::registerCommand(
		"Editor.createWorld",
		[](const vector<string>& arguments)
		{
			Framework* framework = getReplayFramework();
			if (framework == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			if (arguments.size() != 2)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			World* createdWorld = framework->createWorld(arguments[0]);
			if (createdWorld == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::worldCreateFailed);
			}

			createdWorld->setAssetPath(arguments[1]);
			if (!framework->saveActiveWorld())
			{
				return static_cast<int32>(EditorExecutionCode::worldSaveFailed);
			}

			World* loadedWorld = framework->loadWorld(arguments[1]);
			if (loadedWorld == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::worldLoadFailed);
			}

			return editorCommandAreEquivalentAssetPaths(loadedWorld->getAssetPath(), arguments[1])
				? static_cast<int32>(CLIModule::ExecutionCode::succeeded)
				: static_cast<int32>(EditorExecutionCode::assetPathMismatch);
		});
	assert(createWorldRegistered && "[ReplayModule][Assert] reason=editor_create_world_command_register_failed");
	unused(createWorldRegistered);

	const bool addEntityRegistered = CLIModule::registerCommand(
		"Editor.addEntity",
		[](const vector<string>& arguments)
		{
			Framework* framework = getReplayFramework();
			World* world = requireReplayActiveWorld();
			if (framework == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			if (world == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::activeWorldMissing);
			}

			if (arguments.size() != 3)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			const string& expectedEntityAssetPath = arguments[0];
			const string& parentEntityAssetPath = arguments[1];
			const string& entityTypeName = arguments[2];

			uint32 createdEntityIndex = invalidEntityIndex;
			if (entityTypeName == PlaceableEntity::getStaticAssetTypeName())
			{
				createdEntityIndex = world->createPlaceableEntity();
			}
			else if (entityTypeName == Entity::getStaticAssetTypeName())
			{
				createdEntityIndex = world->createEntity();
			}
			else
			{
				return static_cast<int32>(EditorExecutionCode::invalidEntityType);
			}

			if (!parentEntityAssetPath.empty())
			{
				uint32 parentEntityIndex = invalidEntityIndex;
				if (!editorCommandFindEntityIndexByAssetPath(*world, parentEntityAssetPath, parentEntityIndex))
				{
					world->removeEntity(createdEntityIndex);
					return static_cast<int32>(EditorExecutionCode::parentEntityNotFound);
				}

				if (!world->addChildEntity(parentEntityIndex, createdEntityIndex))
				{
					world->removeEntity(createdEntityIndex);
					return static_cast<int32>(EditorExecutionCode::entityReparentFailed);
				}
			}

			if (!framework->saveActiveWorld())
			{
				return static_cast<int32>(EditorExecutionCode::worldSaveFailed);
			}

			uint32 savedEntityIndex = invalidEntityIndex;
			return editorCommandFindEntityIndexByAssetPath(*world, expectedEntityAssetPath, savedEntityIndex)
				? static_cast<int32>(CLIModule::ExecutionCode::succeeded)
				: static_cast<int32>(EditorExecutionCode::assetPathMismatch);
		});
	assert(addEntityRegistered && "[ReplayModule][Assert] reason=editor_add_entity_command_register_failed");
	unused(addEntityRegistered);

	const bool deleteEntityRegistered = CLIModule::registerCommand(
		"Editor.deleteEntity",
		[](const vector<string>& arguments)
		{
			Framework* framework = getReplayFramework();
			World* world = requireReplayActiveWorld();
			if (framework == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			if (world == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::activeWorldMissing);
			}

			if (arguments.size() != 1)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			uint32 entityIndex = invalidEntityIndex;
			if (!editorCommandFindEntityIndexByAssetPath(*world, arguments[0], entityIndex))
			{
				return static_cast<int32>(EditorExecutionCode::entityNotFound);
			}

			if (!world->removeEntity(entityIndex))
			{
				return static_cast<int32>(EditorExecutionCode::entityDeleteFailed);
			}

			if (!framework->saveActiveWorld())
			{
				return static_cast<int32>(EditorExecutionCode::worldSaveFailed);
			}

			return static_cast<int32>(CLIModule::ExecutionCode::succeeded);
		});
	assert(deleteEntityRegistered && "[ReplayModule][Assert] reason=editor_delete_entity_command_register_failed");
	unused(deleteEntityRegistered);

	const bool reparentEntityRegistered = CLIModule::registerCommand(
		"Editor.reparentEntity",
		[](const vector<string>& arguments)
		{
			Framework* framework = getReplayFramework();
			World* world = requireReplayActiveWorld();
			if (framework == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			if (world == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::activeWorldMissing);
			}

			if (arguments.size() != 2)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			uint32 childEntityIndex = invalidEntityIndex;
			if (!editorCommandFindEntityIndexByAssetPath(*world, arguments[0], childEntityIndex))
			{
				return static_cast<int32>(EditorExecutionCode::entityNotFound);
			}

			uint32 parentEntityIndex = invalidEntityIndex;
			if (!arguments[1].empty() && !editorCommandFindEntityIndexByAssetPath(*world, arguments[1], parentEntityIndex))
			{
				return static_cast<int32>(EditorExecutionCode::parentEntityNotFound);
			}

			if (!world->reparentEntity(childEntityIndex, parentEntityIndex))
			{
				return static_cast<int32>(EditorExecutionCode::entityReparentFailed);
			}

			if (!framework->saveActiveWorld())
			{
				return static_cast<int32>(EditorExecutionCode::worldSaveFailed);
			}

			return static_cast<int32>(CLIModule::ExecutionCode::succeeded);
		});
	assert(reparentEntityRegistered && "[ReplayModule][Assert] reason=editor_reparent_entity_command_register_failed");
	unused(reparentEntityRegistered);

	const bool setEntityNameRegistered = CLIModule::registerCommand(
		"Editor.setEntityName",
		[](const vector<string>& arguments)
		{
			Framework* framework = getReplayFramework();
			World* world = requireReplayActiveWorld();
			if (framework == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			if (world == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::activeWorldMissing);
			}

			if (arguments.size() != 2)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			uint32 entityIndex = invalidEntityIndex;
			if (!editorCommandFindEntityIndexByAssetPath(*world, arguments[0], entityIndex))
			{
				return static_cast<int32>(EditorExecutionCode::entityNotFound);
			}

			Entity* entity = world->getEntityByIndex(entityIndex);
			assert(entity != nullptr && "[ReplayModule][Assert] reason=entity_missing");
			entity->setName(arguments[1]);
			if (!framework->saveActiveWorld())
			{
				return static_cast<int32>(EditorExecutionCode::worldSaveFailed);
			}

			return static_cast<int32>(CLIModule::ExecutionCode::succeeded);
		});
	assert(setEntityNameRegistered && "[ReplayModule][Assert] reason=editor_set_entity_name_command_register_failed");
	unused(setEntityNameRegistered);

	const bool addComponentRegistered = CLIModule::registerCommand(
		"Editor.addComponent",
		[](const vector<string>& arguments)
		{
			Framework* framework = getReplayFramework();
			World* world = requireReplayActiveWorld();
			if (framework == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			if (world == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::activeWorldMissing);
			}

			if (arguments.size() != 3)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			uint32 entityIndex = invalidEntityIndex;
			if (!editorCommandFindEntityIndexByAssetPath(*world, arguments[0], entityIndex))
			{
				return static_cast<int32>(EditorExecutionCode::entityNotFound);
			}

			Entity* entity = world->getEntityByIndex(entityIndex);
			assert(entity != nullptr && "[ReplayModule][Assert] reason=entity_missing");

			unique_pointer<Component> component = {};
			if (arguments[1] == MeshComponent::getStaticAssetTypeName())
			{
				component.reset(new MeshComponent());
			}
			else if (arguments[1] == CameraComponent::getStaticAssetTypeName())
			{
				component.reset(new CameraComponent());
			}
			else
			{
				return static_cast<int32>(EditorExecutionCode::invalidComponentType);
			}

			if (!entity->addComponent(moveValue(component)))
			{
				return static_cast<int32>(EditorExecutionCode::componentAddFailed);
			}

			if (!framework->saveActiveWorld())
			{
				return static_cast<int32>(EditorExecutionCode::worldSaveFailed);
			}

			uint32 componentIndex = invalidComponentIndex;
			return editorCommandFindComponentIndexByAssetPath(*world, arguments[2], componentIndex)
				? static_cast<int32>(CLIModule::ExecutionCode::succeeded)
				: static_cast<int32>(EditorExecutionCode::assetPathMismatch);
		});
	assert(addComponentRegistered && "[ReplayModule][Assert] reason=editor_add_component_command_register_failed");
	unused(addComponentRegistered);

	const bool setMeshComponentRegistered = CLIModule::registerCommand(
		"Editor.setMeshComponent",
		[](const vector<string>& arguments)
		{
			Framework* framework = getReplayFramework();
			World* world = requireReplayActiveWorld();
			if (framework == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			if (world == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::activeWorldMissing);
			}

			if (arguments.size() < 4)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			uint32 componentIndex = invalidComponentIndex;
			if (!editorCommandFindComponentIndexByAssetPath(*world, arguments[0], componentIndex))
			{
				return static_cast<int32>(EditorExecutionCode::componentNotFound);
			}

			MeshComponent* meshComponent = componentCast<MeshComponent>(world->getComponentByIndex(componentIndex));
			if (meshComponent == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::invalidComponentType);
			}

			uint32 lodLevel = 0;
			bool visible = true;
			if (!editorCommandParseUnsignedInteger(arguments[2], lodLevel)
				|| !editorCommandParseBoolean(arguments[3], visible))
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			const string& meshAssetPath = arguments[1];
			meshComponent->setMeshAssetPath(meshAssetPath);
			meshComponent->setLODLevel(lodLevel);
			meshComponent->setVisible(visible);
			vector<string> materialAssetPaths = {};
			materialAssetPaths.reserve(arguments.size() > 4 ? arguments.size() - 4 : 0);
			for (uint32 materialIndex = 4; materialIndex < static_cast<uint32>(arguments.size()); ++materialIndex)
			{
				materialAssetPaths.push_back(arguments[materialIndex]);
			}
			meshComponent->setMaterialAssetPaths(materialAssetPaths);
			if (meshAssetPath.empty())
			{
				meshComponent->requestMeshStreaming();
			}
			else
			{
				string absoluteMeshAssetPath = {};
				if (!DiskLoaderModule::get()->resolvePathFromResources(meshAssetPath, absoluteMeshAssetPath))
				{
					return static_cast<int32>(EditorExecutionCode::meshAssetLoadFailed);
				}

				meshComponent->requestMeshStreaming();
			}

			if (!framework->saveActiveWorld())
			{
				return static_cast<int32>(EditorExecutionCode::worldSaveFailed);
			}

			return static_cast<int32>(CLIModule::ExecutionCode::succeeded);
		});
	assert(setMeshComponentRegistered && "[ReplayModule][Assert] reason=editor_set_mesh_component_command_register_failed");
	unused(setMeshComponentRegistered);

	const bool createMaterialAssetRegistered = CLIModule::registerCommand(
		"Editor.createMaterialAsset",
		[](const vector<string>& arguments)
		{
			if (arguments.size() != 7)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			MaterialAsset materialAsset = {};
			materialAsset.clear();
			materialAsset.setAssetPath(arguments[0]);
			materialAsset.setName(arguments[1]);
			materialAsset.setGUID("");
			editorCommandApplyMaterialShaderConfig(
				materialAsset,
				arguments[2],
				arguments[3],
				arguments[4],
				arguments[5],
				arguments[6]);
			if (!saveReplayMaterialAssetDocument(materialAsset))
			{
				return static_cast<int32>(EditorExecutionCode::deassetWriteFailed);
			}

			editorCommandSyncLoadedMaterialShaderConfig(
				requireReplayActiveWorld(),
				arguments[0],
				arguments[2],
				arguments[3],
				arguments[4],
				arguments[5],
				arguments[6]);
			return static_cast<int32>(CLIModule::ExecutionCode::succeeded);
		});
	assert(createMaterialAssetRegistered && "[ReplayModule][Assert] reason=editor_create_material_asset_command_register_failed");
	unused(createMaterialAssetRegistered);

	const bool setMaterialAssetRegistered = CLIModule::registerCommand(
		"Editor.setMaterialAsset",
		[](const vector<string>& arguments)
		{
			if (arguments.size() != 6)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
			if (diskLoaderModule == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			string absoluteAssetPath = {};
			if (!diskLoaderModule->resolveAbsolutePathFromResources(arguments[0], absoluteAssetPath))
			{
				return static_cast<int32>(EditorExecutionCode::deassetReadFailed);
			}

			XMLKeyValueDocument document = {};
			const XML::ParseCode parseCode = XML::get().readDocumentFile(absoluteAssetPath, document);
			if (parseCode != XML::ParseCode::succeeded)
			{
				return static_cast<int32>(EditorExecutionCode::deassetReadFailed);
			}

			std::string_view assetTypeName = {};
			if (!document.tryGetValueView("deasset.@type", assetTypeName)
				|| assetTypeName != MaterialAsset::getStaticAssetTypeName())
			{
				return static_cast<int32>(EditorExecutionCode::deassetReadFailed);
			}

			MaterialAsset materialAsset = {};
			materialAsset.setAssetPath(arguments[0]);
			materialAsset.readProperty(document);
			editorCommandApplyMaterialShaderConfig(
				materialAsset,
				arguments[1],
				arguments[2],
				arguments[3],
				arguments[4],
				arguments[5]);
			if (!saveReplayMaterialAssetDocument(materialAsset))
			{
				return static_cast<int32>(EditorExecutionCode::deassetWriteFailed);
			}

			editorCommandSyncLoadedMaterialShaderConfig(
				requireReplayActiveWorld(),
				arguments[0],
				arguments[1],
				arguments[2],
				arguments[3],
				arguments[4],
				arguments[5]);
			return static_cast<int32>(CLIModule::ExecutionCode::succeeded);
		});
	assert(setMaterialAssetRegistered && "[ReplayModule][Assert] reason=editor_set_material_asset_command_register_failed");
	unused(setMaterialAssetRegistered);

	const bool setCameraComponentRegistered = CLIModule::registerCommand(
		"Editor.setCameraComponent",
		[](const vector<string>& arguments)
		{
			Framework* framework = getReplayFramework();
			World* world = requireReplayActiveWorld();
			if (framework == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			if (world == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::activeWorldMissing);
			}

			if (arguments.size() != 5)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			uint32 componentIndex = invalidComponentIndex;
			if (!editorCommandFindComponentIndexByAssetPath(*world, arguments[0], componentIndex))
			{
				return static_cast<int32>(EditorExecutionCode::componentNotFound);
			}

			CameraComponent* cameraComponent = componentCast<CameraComponent>(world->getComponentByIndex(componentIndex));
			if (cameraComponent == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::invalidComponentType);
			}

			bool primary = false;
			float fieldOfViewYDegrees = 0.0f;
			float nearPlane = 0.0f;
			float farPlane = 0.0f;
			if (!editorCommandParseBoolean(arguments[1], primary)
				|| !editorCommandParseFloat(arguments[2], fieldOfViewYDegrees)
				|| !editorCommandParseFloat(arguments[3], nearPlane)
				|| !editorCommandParseFloat(arguments[4], farPlane))
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			cameraComponent->primary = primary;
			cameraComponent->fieldOfViewYDegrees = editorCommandClampFieldOfViewYDegrees(fieldOfViewYDegrees);
			cameraComponent->nearPlane = nearPlane;
			cameraComponent->farPlane = farPlane;
			editorCommandClampPlanes(cameraComponent->nearPlane, cameraComponent->farPlane);
			if (!framework->saveActiveWorld())
			{
				return static_cast<int32>(EditorExecutionCode::worldSaveFailed);
			}

			return static_cast<int32>(CLIModule::ExecutionCode::succeeded);
		});
	assert(setCameraComponentRegistered && "[ReplayModule][Assert] reason=editor_set_camera_component_command_register_failed");
	unused(setCameraComponentRegistered);

	const bool setTransformRegistered = CLIModule::registerCommand(
		"Editor.setTransform",
		[](const vector<string>& arguments)
		{
			Framework* framework = getReplayFramework();
			World* world = requireReplayActiveWorld();
			if (framework == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			if (world == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::activeWorldMissing);
			}

			if (arguments.size() != 10)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			uint32 entityIndex = invalidEntityIndex;
			if (!editorCommandFindEntityIndexByAssetPath(*world, arguments[0], entityIndex))
			{
				return static_cast<int32>(EditorExecutionCode::entityNotFound);
			}

			PlaceableEntity* placeableEntity = dynamic_cast<PlaceableEntity*>(world->getEntityByIndex(entityIndex));
			if (placeableEntity == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::transformTargetInvalid);
			}

			float values[9] = {};
			for (uint32 valueIndex = 0; valueIndex < 9; ++valueIndex)
			{
				if (!editorCommandParseFloat(arguments[valueIndex + 1], values[valueIndex]))
				{
					return static_cast<int32>(EditorExecutionCode::invalidArguments);
				}
			}

			placeableEntity->transform.positionX = values[0];
			placeableEntity->transform.positionY = values[1];
			placeableEntity->transform.positionZ = values[2];
			placeableEntity->transform.rotationPitch = values[3];
			placeableEntity->transform.rotationYaw = values[4];
			placeableEntity->transform.rotationRoll = values[5];
			placeableEntity->transform.scaleX = values[6];
			placeableEntity->transform.scaleY = values[7];
			placeableEntity->transform.scaleZ = values[8];

			if (!framework->saveActiveWorld())
			{
				return static_cast<int32>(EditorExecutionCode::worldSaveFailed);
			}

			return static_cast<int32>(CLIModule::ExecutionCode::succeeded);
		});
	assert(setTransformRegistered && "[ReplayModule][Assert] reason=editor_set_transform_command_register_failed");
	unused(setTransformRegistered);

	const bool setDeassetPropertyRegistered = CLIModule::registerCommand(
		"Editor.setDeassetProperty",
		[](const vector<string>& arguments)
		{
			if (arguments.size() != 3)
			{
				return static_cast<int32>(EditorExecutionCode::invalidArguments);
			}

			shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
			if (diskLoaderModule == nullptr)
			{
				return static_cast<int32>(EditorExecutionCode::frameworkMissing);
			}

			string absoluteAssetPath = {};
			if (!diskLoaderModule->resolveAbsolutePathFromResources(arguments[0], absoluteAssetPath))
			{
				return static_cast<int32>(EditorExecutionCode::deassetReadFailed);
			}

			XMLKeyValueDocument document = {};
			const XML::ParseCode parseCode = XML::get().readDocumentFile(absoluteAssetPath, document);
			if (parseCode != XML::ParseCode::succeeded)
			{
				return static_cast<int32>(EditorExecutionCode::deassetReadFailed);
			}

			document.set(arguments[1], arguments[2]);
			return XML::get().writeDocumentFile(absoluteAssetPath, document)
				? static_cast<int32>(CLIModule::ExecutionCode::succeeded)
				: static_cast<int32>(EditorExecutionCode::deassetWriteFailed);
		});
	assert(setDeassetPropertyRegistered && "[ReplayModule][Assert] reason=editor_set_deasset_property_command_register_failed");
	unused(setDeassetPropertyRegistered);
}
