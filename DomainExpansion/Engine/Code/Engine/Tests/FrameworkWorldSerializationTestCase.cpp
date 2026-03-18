#include "Engine/Tests/FrameworkWorldSerializationTestCase.h"

#include "Engine/Framework/CameraComponent.h"
#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/FrameworkSerialization.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Framework/World.h"

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

				if (component->getComponentType() == ComponentType::meshComponent)
				{
					loadedMeshComponent = static_cast<MeshComponent*>(component);
				}
				else if (component->getComponentType() == ComponentType::cameraComponent)
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
				if (component == nullptr || component->getComponentType() != ComponentType::cameraComponent)
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
			sceneCameraCount == 1 && editorCameraCount == 1,
			"run: framework load rehydrates scene camera and injects one editor camera") && runResult;
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

	if (loadedWorldIndex != invalidWorldIndex)
	{
		framework.unloadWorld(loadedWorldIndex);
		loadedWorldIndex = invalidWorldIndex;
	}

	generatedWorldFilePath.clear();
	return expectCondition(true, "end: world serialization test cleanup");
}
