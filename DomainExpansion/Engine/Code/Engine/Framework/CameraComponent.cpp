#include "Engine/Framework/CameraComponent.h"

#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/World.h"

static bool isSameMatrix4x4(const float4x4& left, const float4x4& right)
{
	for (uint32 valueIndex = 0; valueIndex < 16; ++valueIndex)
	{
		if (left.value[valueIndex] != right.value[valueIndex])
		{
			return false;
		}
	}

	return true;
}

BridgeHandle CameraComponent::getCameraHandle() const
{
	return cameraHandleReference.getPackedHandle();
}

void CameraComponent::tick(const float deltaTimeSeconds)
{
	unused(deltaTimeSeconds);
	generateCameraBridgeHandle();
}

void CameraComponent::initComponent()
{
	generateCameraBridgeHandle();
}

void CameraComponent::generateCameraBridgeHandle()
{
	if (editorCamera)
	{
		primary = true;
	}

	World* ownerWorld = getOwnerWorld();
	Entity* ownerEntity = ownerWorld != nullptr ? ownerWorld->getEntityByIndex(getOwnerEntityIndex()) : nullptr;
	PlaceableEntity* ownerPlaceableEntity = dynamic_cast<PlaceableEntity*>(ownerEntity);
	assert(ownerPlaceableEntity != nullptr && "[CameraComponent][Assert] reason=camera_requires_placeable_entity");
	if (ownerPlaceableEntity == nullptr)
	{
		cameraHandleReference.reset();
		return;
	}

	const BridgeHandle entityHandle = getOwnerEntityHandle();
	assert(entityHandle != invalidBridgeHandle);
	if (entityHandle == invalidBridgeHandle)
	{
		cameraHandleReference.reset();
		return;
	}

	float3 cameraPosition = {};
	cameraPosition.x = ownerPlaceableEntity->transform.positionX;
	cameraPosition.y = ownerPlaceableEntity->transform.positionY;
	cameraPosition.z = ownerPlaceableEntity->transform.positionZ;
	float3 cameraRotation = {};
	cameraRotation.x = ownerPlaceableEntity->transform.rotationPitch;
	cameraRotation.y = ownerPlaceableEntity->transform.rotationYaw;
	cameraRotation.z = ownerPlaceableEntity->transform.rotationRoll;
	const float4x4 viewMatrix = buildViewMatrix4x4(cameraPosition, cameraRotation);

	bool recreateCameraBridge = !cameraHandleReference.isValid();
	if (!recreateCameraBridge)
	{
		const CameraBridge::StaticData* staticData = CameraBridge::get().getStaticData(cameraHandleReference.getPackedHandle());
		recreateCameraBridge = staticData == nullptr
			|| staticData->entityHandle != entityHandle
			|| staticData->editorCamera != editorCamera;
	}

	if (recreateCameraBridge)
	{
		cameraHandleReference.reset();

		CameraBridge::ObjectDesc cameraObjectDesc = {};
		cameraObjectDesc.staticProperty.entityHandle = entityHandle;
		cameraObjectDesc.staticProperty.editorCamera = editorCamera;
		cameraObjectDesc.dynamicProperty.primary = primary;
		cameraObjectDesc.dynamicProperty.fieldOfViewYDegrees = fieldOfViewYDegrees;
		cameraObjectDesc.dynamicProperty.nearPlane = nearPlane;
		cameraObjectDesc.dynamicProperty.farPlane = farPlane;
		cameraObjectDesc.dynamicProperty.viewMatrix = viewMatrix;
		cameraHandleReference = CameraBridge::get().createCameraHandle(cameraObjectDesc);
		assert(cameraHandleReference.isValid());
		return;
	}

	const CameraBridge::DynamicData* dynamicData = CameraBridge::get().getDynamicData(cameraHandleReference.getPackedHandle());
	if (dynamicData != nullptr
		&& dynamicData->primary == primary
		&& dynamicData->fieldOfViewYDegrees == fieldOfViewYDegrees
		&& dynamicData->nearPlane == nearPlane
		&& dynamicData->farPlane == farPlane
		&& isSameMatrix4x4(dynamicData->viewMatrix, viewMatrix))
	{
		return;
	}

	CameraBridge::DynamicData nextDynamicData = {};
	nextDynamicData.primary = primary;
	nextDynamicData.fieldOfViewYDegrees = fieldOfViewYDegrees;
	nextDynamicData.nearPlane = nearPlane;
	nextDynamicData.farPlane = farPlane;
	nextDynamicData.viewMatrix = viewMatrix;
	CameraBridge::get().updateDynamicData(cameraHandleReference.getPackedHandle(), nextDynamicData);
}
