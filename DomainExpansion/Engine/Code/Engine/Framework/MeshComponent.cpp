#include "Engine/Framework/MeshComponent.h"

#include "Engine/Common/XML/XML.h"
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
	lodLevel = 0;
	visible = true;
	meshHandleReference.reset();
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

void MeshComponent::writeAssetProperty(OutputFileStream& fileStream) const
{
	Component::writeAssetProperty(fileStream);

	if (!meshAssetPath.empty())
	{
		assert(meshAsset != nullptr && "[MeshComponent][Assert] reason=mesh_asset_missing");
		assert(meshAsset->getAssetPath() == meshAssetPath && "[MeshComponent][Assert] reason=mesh_asset_path_mismatch");
	}

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
	if (meshAssetPath.empty())
	{
		return;
	}

	const XMLKeyValueDocument meshAssetDocument = XML::get().readDocumentFile(meshAssetPath);
	MeshAsset loadedMeshAsset = {};
	loadedMeshAsset.setAssetPath(meshAssetPath);
	loadedMeshAsset.readProperty(meshAssetDocument);
	meshAsset = shared_pointer<MeshAsset>(new MeshAsset(moveValue(loadedMeshAsset)));
}

void MeshComponent::generateMeshBridgeHandle()
{
	const bool needsMeshBridge = !meshAssetPath.empty();
	if (!needsMeshBridge)
	{
		meshAsset.reset();
		meshHandleReference.reset();
		return;
	}

	assert(meshAsset != nullptr && "[MeshComponent][Assert] reason=mesh_asset_missing");
	assert(meshAsset->getAssetPath() == meshAssetPath && "[MeshComponent][Assert] reason=mesh_asset_path_mismatch");

	const BridgeHandle entityHandle = getOwnerEntityHandle();
	assert(entityHandle != invalidBridgeHandle);

	shared_pointer<MeshAssetHandle> meshAssetHandle = MeshStreaming::get()->requestMesh(meshAsset, lodLevel);
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
