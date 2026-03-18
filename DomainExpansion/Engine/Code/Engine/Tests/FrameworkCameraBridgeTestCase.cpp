#include "Engine/Tests/FrameworkCameraBridgeTestCase.h"

#include "Bridge/CameraBridge.h"
#include "Bridge/EntityBridge.h"
#include "Engine/Framework/CameraComponent.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Framework/PlaceableEntity.h"

static CameraComponent* getEditorCameraComponent(World* world, uint32& outEntityIndex, PlaceableEntity*& outEntity)
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
			if (component == nullptr || component->getComponentType() != ComponentType::cameraComponent)
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

const char* FrameworkCameraBridgeTestCase::getTestCaseName() const
{
	return "FrameworkCameraBridgeTestCase";
}

bool FrameworkCameraBridgeTestCase::beginTest(Framework& framework)
{
	worldIndex = framework.createWorld(L"FrameworkCameraBridge");
	return expectCondition(framework.loadWorld(worldIndex), "begin: load camera bridge world");
}

bool FrameworkCameraBridgeTestCase::runTest(Framework& framework)
{
	World* activeWorld = framework.getActiveWorld();
	bool runResult = true;
	runResult = expectCondition(activeWorld != nullptr, "run: active world exists") && runResult;
	if (activeWorld == nullptr)
	{
		return false;
	}

	uint32 editorCameraEntityIndex = invalidEntityIndex;
	PlaceableEntity* editorCameraEntity = nullptr;
	CameraComponent* editorCameraComponent = getEditorCameraComponent(activeWorld, editorCameraEntityIndex, editorCameraEntity);
	runResult = expectCondition(
		editorCameraComponent != nullptr
			&& editorCameraEntity != nullptr
			&& editorCameraComponent->primary
			&& editorCameraComponent->fieldOfViewYDegrees == 60.0f
			&& editorCameraComponent->nearPlane == 0.1f
			&& editorCameraComponent->farPlane == 100.0f
			&& editorCameraEntity->transform.positionZ == 4.0f,
		"run: editor camera component created with default transform and projection") && runResult;
	if (editorCameraComponent == nullptr)
	{
		return false;
	}

	const BridgeHandle cameraHandle = editorCameraComponent->getCameraHandle();
	const CameraBridge::StaticData* cameraStaticData = CameraBridge::get().getStaticData(cameraHandle);
	const CameraBridge::DynamicData* cameraDynamicData = CameraBridge::get().getDynamicData(cameraHandle);
	const EntityBridge::DynamicData* entityDynamicData = cameraStaticData != nullptr
		? EntityBridge::get().getDynamicData(cameraStaticData->entityHandle)
		: nullptr;
	runResult = expectCondition(
		cameraHandle != invalidBridgeHandle
			&& cameraStaticData != nullptr
			&& cameraStaticData->editorCamera
			&& cameraDynamicData != nullptr
			&& cameraDynamicData->primary
			&& entityDynamicData != nullptr
			&& entityDynamicData->active
			&& entityDynamicData->hasTransform
			&& entityDynamicData->transform.positionZ == 4.0f
			&& cameraDynamicData->viewMatrix.value[14] == 4.0f,
		"run: editor camera bridge and entity bridge synced") && runResult;

	const bool reloadResult = framework.loadWorld(worldIndex);
	runResult = expectCondition(reloadResult, "run: reload world reuses editor camera") && runResult;
	if (!reloadResult)
	{
		return false;
	}
	activeWorld = framework.getActiveWorld();

	uint32 editorCameraCount = 0;
	for (uint32 entityIndex = 0; entityIndex < activeWorld->getEntityCount(); ++entityIndex)
	{
		Entity* entity = activeWorld->getEntityByIndex(entityIndex);
		if (entity == nullptr)
		{
			continue;
		}

		for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
		{
			Component* component = activeWorld->getComponentByIndex(entity->getComponentIndex(componentArrayIndex));
			if (component == nullptr || component->getComponentType() != ComponentType::cameraComponent)
			{
				continue;
			}

			CameraComponent* cameraComponent = static_cast<CameraComponent*>(component);
			if (cameraComponent->editorCamera)
			{
				++editorCameraCount;
			}
		}
	}

	runResult = expectCondition(editorCameraCount == 1, "run: editor camera is not duplicated on reload") && runResult;
	return runResult;
}

bool FrameworkCameraBridgeTestCase::endTest(Framework& framework)
{
	framework.unloadWorld(worldIndex);
	worldIndex = invalidWorldIndex;
	return expectCondition(true, "end: camera bridge cleanup");
}
