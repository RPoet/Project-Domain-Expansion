#include "Engine/Framework/MeshComponent.h"

#include "Engine/Assets/AssetLoader.h"
#include "Engine/Module/MeshStreaming/MeshStreaming.h"

BridgeHandle MeshComponent::getMeshHandle() const
{
	return meshHandleReference.getPackedHandle();
}

void MeshComponent::clear()
{
	Component::clear();
	meshAssetPath.clear();
	meshAsset.reset();
	meshAssetHandle.reset();
	lodLevel = 0;
	visible = true;
	meshHandleReference.reset();
}

void MeshComponent::tick(const float deltaTimeSeconds)
{
	unused(deltaTimeSeconds);
	generateMeshBridgeHandle();
}

void MeshComponent::initialize()
{
	generateMeshBridgeHandle();
}

void MeshComponent::writeAssetProperty(OutputFileStream& fileStream) const
{
	Component::writeAssetProperty(fileStream);

	XML& xml = XML::get();
	xml.writeProperty(fileStream, "meshAssetPath", meshAssetPath);
	xml.writeProperty(fileStream, "lodLevel", lodLevel);
	xml.writeProperty(fileStream, "visible", visible);
}

void MeshComponent::readAssetProperty(const XMLKeyValueDocument& document)
{
	Component::readAssetProperty(document);

	XML& xml = XML::get();
	xml.readProperty(document, "deasset.meshAssetPath", meshAssetPath);
	xml.readProperty(document, "deasset.lodLevel", lodLevel);
	xml.readProperty(document, "deasset.visible", visible);

	meshAsset.reset();
	meshAssetHandle.reset();
	if (meshAssetPath.empty())
	{
		return;
	}

	meshAsset = AssetLoader::get().loadSharedAsset<MeshAsset>(meshAssetPath);
}

void MeshComponent::generateMeshBridgeHandle()
{
	const bool needsMeshBridge = !meshAssetPath.empty() && meshAsset != nullptr;
	if (!needsMeshBridge)
	{
		meshAsset.reset();
		meshAssetHandle.reset();
		meshHandleReference.reset();
		return;
	}

	const BridgeHandle entityHandle = getOwnerEntityHandle();
	assert(entityHandle != invalidBridgeHandle);

	if (meshAssetHandle == nullptr || meshAssetHandle->meshAsset != meshAsset || meshAssetHandle->lodLevel != lodLevel)
	{
		meshAssetHandle = MeshStreaming::get()->requestMesh(meshAsset, lodLevel);
	}
	assert(meshAssetHandle != nullptr);

	bool recreateMeshBridge = !meshHandleReference.isValid();
	if (!recreateMeshBridge)
	{
		const MeshBridge::StaticData* staticData = MeshBridge::get().getStaticData(meshHandleReference.getPackedHandle());
		recreateMeshBridge = staticData == nullptr
			|| staticData->entityHandle != entityHandle
			|| staticData->meshAssetPath != meshAssetPath
			|| staticData->lodLevel != lodLevel;
	}

	if (recreateMeshBridge)
	{
		meshHandleReference.reset();

		MeshBridge::ObjectDesc meshObjectDesc = {};
		meshObjectDesc.staticProperty.entityHandle = entityHandle;
		meshObjectDesc.staticProperty.meshAssetPath = meshAssetPath;
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
