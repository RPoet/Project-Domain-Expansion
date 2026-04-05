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

void CameraComponent::clear()
{
	Component::clear();
	editorCamera = false;
	primary = false;
	fieldOfViewYDegrees = 60.0f;
	nearPlane = 0.1f;
	farPlane = 100.0f;
	cameraHandleReference.reset();
}

void CameraComponent::tick(const float deltaTimeSeconds)
{
	unused(deltaTimeSeconds);
	generateCameraBridgeHandle();
}

void CameraComponent::initialize()
{
	generateCameraBridgeHandle();
}

void CameraComponent::writeAssetProperty(OutputFileStream& fileStream) const
{
	Component::writeAssetProperty(fileStream);

	XML& xml = XML::get();
	xml.writeProperty(fileStream, "editorCamera", editorCamera);
	xml.writeProperty(fileStream, "primary", primary);
	xml.writeProperty(fileStream, "fieldOfViewYDegrees", fieldOfViewYDegrees);
	xml.writeProperty(fileStream, "nearPlane", nearPlane);
	xml.writeProperty(fileStream, "farPlane", farPlane);
}

void CameraComponent::readAssetProperty(const XMLKeyValueDocument& document)
{
	Component::readAssetProperty(document);

	XML& xml = XML::get();
	xml.readProperty(document, "deasset.editorCamera", editorCamera);
	xml.readProperty(document, "deasset.primary", primary);
	xml.readProperty(document, "deasset.fieldOfViewYDegrees", fieldOfViewYDegrees);
	xml.readProperty(document, "deasset.nearPlane", nearPlane);
	xml.readProperty(document, "deasset.farPlane", farPlane);
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
	assert(ownerWorld != nullptr && "[CameraComponent][Assert] reason=owner_world_missing");

	const BridgeHandle entityHandle = getOwnerEntityHandle();
	assert(entityHandle != invalidBridgeHandle);

	// TODO: Remove this TEMP_ world-transform query after Entity caches world transform directly.
	Transform worldTransform = {};
	const bool hasWorldTransform = ownerWorld->TEMP_tryGetEntityWorldTransform(getOwnerEntityIndex(), worldTransform);
	assert(hasWorldTransform && "[CameraComponent][Assert] reason=world_transform_build_failed");

	float3 cameraPosition = {};
	cameraPosition.x = worldTransform.positionX;
	cameraPosition.y = worldTransform.positionY;
	cameraPosition.z = worldTransform.positionZ;
	float3 cameraRotation = {};
	cameraRotation.x = worldTransform.rotationPitch;
	cameraRotation.y = worldTransform.rotationYaw;
	cameraRotation.z = worldTransform.rotationRoll;
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
