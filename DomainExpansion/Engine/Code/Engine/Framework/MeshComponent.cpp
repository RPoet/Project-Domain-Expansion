#include "Engine/Framework/MeshComponent.h"

#include "Engine/Module/Asset/MeshStreaming.h"

BridgeHandle MeshComponent::getMeshHandle() const
{
	return meshHandleReference.getPackedHandle();
}

void MeshComponent::tick(const float deltaTimeSeconds)
{
	unused(deltaTimeSeconds);
	generateMeshBridgeHandle();
}

void MeshComponent::initComponent()
{
	generateMeshBridgeHandle();
}

void MeshComponent::generateMeshBridgeHandle()
{
	const bool needsMeshBridge = !meshRelativePath.empty();
	if (!needsMeshBridge)
	{
		meshHandleReference.reset();
		return;
	}

	const BridgeHandle entityHandle = getOwnerEntityHandle();
	assert(entityHandle != invalidBridgeHandle);
	if (entityHandle == invalidBridgeHandle)
	{
		meshHandleReference.reset();
		return;
	}

	shared_pointer<MeshAssetHandle> meshAssetHandle = MeshStreaming::get()->requestMesh(meshRelativePath, lodLevel);
	assert(meshAssetHandle != nullptr);
	if (meshAssetHandle == nullptr)
	{
		meshHandleReference.reset();
		return;
	}

	bool recreateMeshBridge = !meshHandleReference.isValid();
	if (!recreateMeshBridge)
	{
		const MeshBridge::StaticData* staticData = MeshBridge::get().getStaticData(meshHandleReference.getPackedHandle());
		recreateMeshBridge = staticData == nullptr
			|| staticData->entityHandle != entityHandle
			|| staticData->meshRelativePath != meshRelativePath
			|| staticData->lodLevel != lodLevel;
	}

	if (recreateMeshBridge)
	{
		meshHandleReference.reset();

		MeshBridge::ObjectDesc meshObjectDesc = {};
		meshObjectDesc.staticProperty.entityHandle = entityHandle;
		meshObjectDesc.staticProperty.meshRelativePath = meshRelativePath;
		meshObjectDesc.staticProperty.lodLevel = lodLevel;
		meshObjectDesc.staticProperty.meshAssetHandle = meshAssetHandle;
		meshObjectDesc.dynamicProperty.visible = visible;
		meshHandleReference = MeshBridge::get().createMeshHandle(meshObjectDesc);
		assert(meshHandleReference.isValid());
		return;
	}

	const MeshBridge::DynamicData* dynamicData = MeshBridge::get().getDynamicData(meshHandleReference.getPackedHandle());
	if (dynamicData == nullptr || dynamicData->visible == visible)
	{
		return;
	}

	MeshBridge::DynamicData nextDynamicData = *dynamicData;
	nextDynamicData.visible = visible;
	MeshBridge::get().updateDynamicData(meshHandleReference.getPackedHandle(), nextDynamicData);
}
