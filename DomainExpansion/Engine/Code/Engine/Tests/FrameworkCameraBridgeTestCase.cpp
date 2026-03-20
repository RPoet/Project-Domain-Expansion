#include "Engine/Tests/FrameworkCameraBridgeTestCase.h"

#include "Bridge/CameraBridge.h"
#include "Bridge/EntityBridge.h"
#include "Engine/Framework/CameraComponent.h"
#include "Engine/Framework/EditorCameraMovementComponent.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Platform/SIMDMath.h"

#include <math.h>

static bool isNearlyEqualFloat(const float left, const float right, const float epsilon = 0.0001f)
{
	return fabsf(left - right) <= epsilon;
}

static bool isNearlyIdentityMatrix(const float4x4& matrix, const float epsilon = 0.0001f)
{
	for (uint32 rowIndex = 0; rowIndex < 4; ++rowIndex)
	{
		for (uint32 columnIndex = 0; columnIndex < 4; ++columnIndex)
		{
			const float expectedValue = rowIndex == columnIndex ? 1.0f : 0.0f;
			if (!isNearlyEqualFloat(matrix.value[rowIndex * 4 + columnIndex], expectedValue, epsilon))
			{
				return false;
			}
		}
	}

	return true;
}

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
	EditorCameraMovementComponent* editorCameraMovementComponent =
		getEditorCameraMovementComponent(activeWorld, editorCameraEntityIndex);
	runResult = expectCondition(
		editorCameraComponent != nullptr
			&& editorCameraEntity != nullptr
			&& editorCameraMovementComponent != nullptr
			&& editorCameraComponent->primary
			&& editorCameraComponent->fieldOfViewYDegrees == 60.0f
			&& editorCameraComponent->nearPlane == 0.1f
			&& editorCameraComponent->farPlane == 100.0f
			&& editorCameraMovementComponent->getMovementSpeed() == 4.0f
			&& editorCameraEntity->transform.positionZ == -4.0f,
		"run: editor camera components created with default transform, projection, and movement speed") && runResult;
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
			&& entityDynamicData->transform.positionZ == -4.0f
			&& cameraDynamicData->viewMatrix.value[14] == 4.0f,
		"run: editor camera bridge and entity bridge synced") && runResult;
	if (!runResult)
	{
		return false;
	}
	const float3 testPosition = { 2.0f, -3.0f, 5.0f };
	const float3 testRotation = { 0.5f, 1.0f, -0.25f };
	const float3 testScale = { 1.0f, 1.0f, 1.0f };
	const float4x4 testWorldMatrix = buildWorldMatrix4x4(testPosition, testRotation, testScale);
	const float4x4 testViewMatrix = buildViewMatrix4x4(testPosition, testRotation);
	const float4x4 combinedMatrix = multiplyMatrix4x4(testWorldMatrix, testViewMatrix);
	runResult = expectCondition(
		isNearlyIdentityMatrix(combinedMatrix),
		"run: camera view matrix is the inverse of the camera world transform") && runResult;
	if (!runResult)
	{
		return false;
	}

	const bool reloadResult = framework.loadWorld(worldIndex);
	runResult = expectCondition(reloadResult, "run: reload world reuses editor camera") && runResult;
	if (!reloadResult)
	{
		return false;
	}
	activeWorld = framework.getActiveWorld();
	editorCameraComponent = getEditorCameraComponent(activeWorld, editorCameraEntityIndex, editorCameraEntity);
	editorCameraMovementComponent = getEditorCameraMovementComponent(activeWorld, editorCameraEntityIndex);

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
			if (component == nullptr || component->getComponentType() != CameraComponent::staticComponentType)
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

	runResult = expectCondition(
		editorCameraCount == 1
			&& editorCameraComponent != nullptr
			&& editorCameraMovementComponent != nullptr,
		"run: editor camera is not duplicated on reload and keeps movement component") && runResult;
	return runResult;
}

bool FrameworkCameraBridgeTestCase::endTest(Framework& framework)
{
	framework.unloadWorld(worldIndex);
	worldIndex = invalidWorldIndex;
	return expectCondition(true, "end: camera bridge cleanup");
}
