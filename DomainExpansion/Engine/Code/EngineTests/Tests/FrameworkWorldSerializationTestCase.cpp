#include "EngineTests/Tests/FrameworkWorldSerializationTestCase.h"

#include "Engine/Framework/CameraComponent.h"
#include "Engine/Framework/EditorCameraMovementComponent.h"
#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/FrameworkSerialization.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Framework/World.h"

static CameraComponent* getFirstEditorCameraComponent(World* world, uint32& outEntityIndex, PlaceableEntity*& outEntity)
{
	outEntityIndex = invalidEntityIndex;
	outEntity = nullptr;
	if (world == nullptr)
	{
		return nullptr;
	}

	for (uint32 entityIndex = 0; entityIndex < world->getEntityCount(); ++entityIndex)
	{
		Entity* entity = world->getEntityByIndex(entityIndex);
		if (entity == nullptr)
		{
			continue;
		}

		for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
		{
			Component* component = world->getComponentByIndex(entity->getComponentIndex(componentArrayIndex));
			if (component == nullptr || component->getComponentType() != CameraComponent::staticComponentType)
			{
				continue;
			}

			CameraComponent* cameraComponent = static_cast<CameraComponent*>(component);
			if (!cameraComponent->editorCamera)
			{
				continue;
			}

			outEntityIndex = entityIndex;
			outEntity = dynamic_cast<PlaceableEntity*>(entity);
			return cameraComponent;
		}
	}

	return nullptr;
}

static EditorCameraMovementComponent* getEditorCameraMovementComponent(World* world, const uint32 entityIndex)
{
	if (world == nullptr || entityIndex == invalidEntityIndex)
	{
		return nullptr;
	}

	Entity* entity = world->getEntityByIndex(entityIndex);
	if (entity == nullptr)
	{
		return nullptr;
	}

	for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
	{
		Component* component = world->getComponentByIndex(entity->getComponentIndex(componentArrayIndex));
		if (component == nullptr || component->getComponentType() != EditorCameraMovementComponent::staticComponentType)
		{
			continue;
		}

		return static_cast<EditorCameraMovementComponent*>(component);
	}

	return nullptr;
}

const char* FrameworkWorldSerializationTestCase::getTestCaseName() const
{
	return "FrameworkWorldSerializationTestCase";
}

bool FrameworkWorldSerializationTestCase::beginTest(Framework& framework)
{
	unused(framework);
	std::error_code pathError;
	const filesystem_path currentPath = std::filesystem::current_path(pathError);
	if (pathError)
	{
		return expectCondition(false, "begin: resolve current path");
	}

	generatedWorldFilePath = (currentPath / "FrameworkWorldSerializationTest.world").string();
	generatedEditorWorldFilePath = (currentPath / "FrameworkEditorWorldSerializationTest.world").string();
	return expectCondition(true, "begin: setup generated world path");
}

bool FrameworkWorldSerializationTestCase::runTest(Framework& framework)
{
	World sourceWorld(L"SerializationRoundTrip");
	const uint32 entityIndex = sourceWorld.createPlaceableEntity();
	Entity* sourceEntity = sourceWorld.getEntityByIndex(entityIndex);
	bool runResult = true;
	runResult = expectCondition(
		sourceEntity != nullptr,
		"run: create source placeable entity") && runResult;
	if (sourceEntity != nullptr)
	{
		sourceEntity->setName("SerializationEntity");
	}

	PlaceableEntity* sourcePlaceableEntity = dynamic_cast<PlaceableEntity*>(sourceEntity);
	if (sourcePlaceableEntity != nullptr)
	{
		sourcePlaceableEntity->transform.positionX = 1.0f;
		sourcePlaceableEntity->transform.positionY = 2.0f;
		sourcePlaceableEntity->transform.positionZ = 3.0f;
	}

	if (sourceEntity != nullptr)
	{
		unique_pointer<MeshComponent> meshComponent(new MeshComponent());
		meshComponent->meshRelativePath = "Meshes/Sphere.obj";
		meshComponent->lodLevel = 2;
		meshComponent->visible = true;
		runResult = expectCondition(
			sourceEntity->addComponent(moveValue(meshComponent)),
			"run: add mesh component to source entity") && runResult;

		unique_pointer<CameraComponent> cameraComponent(new CameraComponent());
		cameraComponent->primary = true;
		cameraComponent->fieldOfViewYDegrees = 75.0f;
		cameraComponent->nearPlane = 0.5f;
		cameraComponent->farPlane = 250.0f;
		runResult = expectCondition(
			sourceEntity->addComponent(moveValue(cameraComponent)),
			"run: add camera component to source entity") && runResult;
	}

	string saveErrorText = {};
	runResult = expectCondition(
		frameworkSerializationSaveWorldToFile(sourceWorld, generatedWorldFilePath, saveErrorText),
		"run: save source world to file") && runResult;

	unique_pointer<World> loadedWorld = nullptr;
	string loadErrorText = {};
	runResult = expectCondition(
		frameworkSerializationLoadWorldFromFile(generatedWorldFilePath, loadedWorld, loadErrorText),
		"run: load world from saved file") && runResult;
	runResult = expectCondition(
		loadedWorld != nullptr && loadedWorld->getEntityCount() == 1,
		"run: loaded world has one entity") && runResult;

	if (loadedWorld != nullptr)
	{
		Entity* loadedEntity = loadedWorld->getEntityByIndex(0);
		runResult = expectCondition(
			loadedEntity != nullptr,
			"run: loaded entity lookup succeeds") && runResult;
		if (loadedEntity != nullptr)
		{
			runResult = expectCondition(
				loadedEntity->getName() == "SerializationEntity",
				"run: loaded entity name preserved") && runResult;
		}

		MeshComponent* loadedMeshComponent = nullptr;
		CameraComponent* loadedCameraComponent = nullptr;
		if (loadedEntity != nullptr)
		{
			for (uint32 componentArrayIndex = 0;
				componentArrayIndex < loadedEntity->getComponentCount();
				++componentArrayIndex)
			{
				const uint32 componentIndex = loadedEntity->getComponentIndex(componentArrayIndex);
				Component* component = loadedWorld->getComponentByIndex(componentIndex);
				if (component == nullptr)
				{
					continue;
				}

				if (component->getComponentType() == MeshComponent::staticComponentType)
				{
					loadedMeshComponent = static_cast<MeshComponent*>(component);
				}
				else if (component->getComponentType() == CameraComponent::staticComponentType)
				{
					loadedCameraComponent = static_cast<CameraComponent*>(component);
				}
			}
		}

		runResult = expectCondition(
			loadedMeshComponent != nullptr,
			"run: loaded mesh component exists") && runResult;
		if (loadedMeshComponent != nullptr)
		{
			runResult = expectCondition(
				loadedMeshComponent->meshRelativePath == "Meshes/Sphere.obj"
				&& loadedMeshComponent->lodLevel == 2
				&& loadedMeshComponent->visible,
				"run: loaded mesh component fields preserved") && runResult;
		}

		runResult = expectCondition(
			loadedCameraComponent != nullptr && !loadedCameraComponent->editorCamera,
			"run: loaded camera component exists and is not editor camera") && runResult;
		if (loadedCameraComponent != nullptr)
		{
			runResult = expectCondition(
				loadedCameraComponent->primary
					&& loadedCameraComponent->fieldOfViewYDegrees == 75.0f
					&& loadedCameraComponent->nearPlane == 0.5f
					&& loadedCameraComponent->farPlane == 250.0f,
				"run: loaded camera component fields preserved") && runResult;
		}
	}

	World editorSourceWorld(L"EditorSerializationRoundTrip");
	const uint32 editorEntityIndex = editorSourceWorld.createPlaceableEntity();
	PlaceableEntity* editorPlaceableEntity =
		static_cast<PlaceableEntity*>(editorSourceWorld.getEntityByIndex(editorEntityIndex));
	runResult = expectCondition(editorPlaceableEntity != nullptr, "run: create editor camera entity") && runResult;
	if (editorPlaceableEntity != nullptr)
	{
		editorPlaceableEntity->setName("EditorCamera");
		editorPlaceableEntity->transform.positionZ = -4.0f;

		unique_pointer<CameraComponent> editorCameraComponent(new CameraComponent());
		editorCameraComponent->editorCamera = true;
		editorCameraComponent->primary = true;
		runResult = expectCondition(
			editorPlaceableEntity->addComponent(moveValue(editorCameraComponent)),
			"run: add editor camera component to editor world") && runResult;

		unique_pointer<EditorCameraMovementComponent> editorCameraMovementComponent(new EditorCameraMovementComponent());
		editorCameraMovementComponent->setMovementSpeed(8.0f);
		runResult = expectCondition(
			editorPlaceableEntity->addComponent(moveValue(editorCameraMovementComponent)),
			"run: add editor camera movement component to editor world") && runResult;
	}

	string editorSaveErrorText = {};
	runResult = expectCondition(
		frameworkSerializationSaveWorldToFile(editorSourceWorld, generatedEditorWorldFilePath, editorSaveErrorText),
		"run: save editor world to file") && runResult;

	unique_pointer<World> editorLoadedWorld = nullptr;
	string editorLoadErrorText = {};
	runResult = expectCondition(
		frameworkSerializationLoadWorldFromFile(generatedEditorWorldFilePath, editorLoadedWorld, editorLoadErrorText),
		"run: load editor world from saved file") && runResult;
	if (editorLoadedWorld != nullptr)
	{
		uint32 loadedEditorEntityIndex = invalidEntityIndex;
		PlaceableEntity* loadedEditorEntity = nullptr;
		CameraComponent* loadedEditorCameraComponent =
			getFirstEditorCameraComponent(editorLoadedWorld.get(), loadedEditorEntityIndex, loadedEditorEntity);
		EditorCameraMovementComponent* loadedEditorCameraMovementComponent =
			getEditorCameraMovementComponent(editorLoadedWorld.get(), loadedEditorEntityIndex);
		runResult = expectCondition(
			loadedEditorCameraComponent != nullptr
				&& loadedEditorEntity != nullptr
				&& loadedEditorCameraMovementComponent != nullptr
				&& loadedEditorCameraComponent->primary
				&& loadedEditorCameraMovementComponent->getMovementSpeed() == 8.0f
				&& loadedEditorEntity->transform.positionZ == -4.0f,
			"run: direct serialization preserves editor camera and movement component") && runResult;
	}

	runResult = expectCondition(
		framework.loadWorldFromFile(generatedEditorWorldFilePath),
		"run: framework can load serialized editor world file as-authored") && runResult;
	if (framework.getActiveWorld() != nullptr)
	{
		loadedWorldIndex = framework.getActiveWorldIndex();
		uint32 sceneCameraCount = 0;
		uint32 editorCameraCount = 0;
		uint32 activeEditorCameraEntityIndex = invalidEntityIndex;
		PlaceableEntity* activeEditorCameraEntity = nullptr;
		CameraComponent* activeEditorCameraComponent =
			getFirstEditorCameraComponent(framework.getActiveWorld(), activeEditorCameraEntityIndex, activeEditorCameraEntity);
		EditorCameraMovementComponent* activeEditorCameraMovementComponent =
			getEditorCameraMovementComponent(framework.getActiveWorld(), activeEditorCameraEntityIndex);
		for (uint32 entityIndex = 0; entityIndex < framework.getActiveWorld()->getEntityCount(); ++entityIndex)
		{
			Entity* entity = framework.getActiveWorld()->getEntityByIndex(entityIndex);
			if (entity == nullptr)
			{
				continue;
			}

			for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
			{
				Component* component = framework.getActiveWorld()->getComponentByIndex(entity->getComponentIndex(componentArrayIndex));
				if (component == nullptr || component->getComponentType() != CameraComponent::staticComponentType)
				{
					continue;
				}

				CameraComponent* cameraComponent = static_cast<CameraComponent*>(component);
				if (cameraComponent->editorCamera)
				{
					++editorCameraCount;
				}
				else
				{
					++sceneCameraCount;
				}
			}
		}

		runResult = expectCondition(
			sceneCameraCount == 0
				&& editorCameraCount == 1
				&& activeEditorCameraComponent != nullptr
				&& activeEditorCameraMovementComponent != nullptr,
			"run: framework preserves serialized editor world camera state without mutation") && runResult;
	}

	if (loadedWorldIndex != invalidWorldIndex)
	{
		framework.unloadWorld(loadedWorldIndex);
		loadedWorldIndex = invalidWorldIndex;
	}

	runResult = expectCondition(
		framework.loadWorldFromFile(generatedWorldFilePath),
		"run: framework can load serialized world file") && runResult;
	if (framework.getActiveWorld() != nullptr)
	{
		loadedWorldIndex = framework.getActiveWorldIndex();
		uint32 sceneCameraCount = 0;
		uint32 editorCameraCount = 0;
		for (uint32 entityIndex = 0; entityIndex < framework.getActiveWorld()->getEntityCount(); ++entityIndex)
		{
			Entity* entity = framework.getActiveWorld()->getEntityByIndex(entityIndex);
			if (entity == nullptr)
			{
				continue;
			}

			for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
			{
				Component* component = framework.getActiveWorld()->getComponentByIndex(entity->getComponentIndex(componentArrayIndex));
				if (component == nullptr || component->getComponentType() != CameraComponent::staticComponentType)
				{
					continue;
				}

				CameraComponent* cameraComponent = static_cast<CameraComponent*>(component);
				if (cameraComponent->editorCamera)
				{
					++editorCameraCount;
				}
				else
				{
					++sceneCameraCount;
				}
			}
		}

		runResult = expectCondition(
			sceneCameraCount == 1 && editorCameraCount == 0,
			"run: framework load preserves non-editor world camera state without editor camera injection") && runResult;
	}

	return runResult;
}

bool FrameworkWorldSerializationTestCase::endTest(Framework& framework)
{
	std::error_code removeError;
	if (!generatedWorldFilePath.empty())
	{
		std::filesystem::remove(generatedWorldFilePath, removeError);
	}
	if (!generatedEditorWorldFilePath.empty())
	{
		std::filesystem::remove(generatedEditorWorldFilePath, removeError);
	}

	if (loadedWorldIndex != invalidWorldIndex)
	{
		framework.unloadWorld(loadedWorldIndex);
		loadedWorldIndex = invalidWorldIndex;
	}

	generatedWorldFilePath.clear();
	generatedEditorWorldFilePath.clear();
	return expectCondition(true, "end: world serialization test cleanup");
}
