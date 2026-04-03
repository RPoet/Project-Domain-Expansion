#include "Engine/Framework/MeshComponent.h"

#include "Engine/Assets/AssetLoader.h"
#include "Engine/Module/MeshStreaming/MeshStreaming.h"

BridgeHandle MeshComponent::getMeshHandle() const
{
	return meshHandleReference.getPackedHandle();
}

const string& MeshComponent::getMeshAssetPath() const
{
	return meshAssetPath;
}

shared_pointer<MeshAsset> MeshComponent::getLoadedMeshAsset() const
{
	return meshAsset;
}

uint32 MeshComponent::getLODLevel() const
{
	return lodLevel;
}

bool MeshComponent::isVisible() const
{
	return visible;
}

bool MeshComponent::hasLoadedMeshAsset() const
{
	return meshAsset != nullptr;
}

uint32 MeshComponent::getMeshSectionCount() const
{
	if (meshAsset == nullptr || lodLevel >= meshAsset->getLODCount())
	{
		return 0;
	}

	return static_cast<uint32>(meshAsset->getSectionRanges(lodLevel).size());
}

const vector<string>& MeshComponent::getMaterialAssetPaths() const
{
	return materialAssetPaths;
}

shared_pointer<MaterialAsset> MeshComponent::getMaterialAsset(const uint32 sectionIndex) const
{
	return sectionIndex < materialAssets.size() ? materialAssets[sectionIndex] : nullptr;
}

void MeshComponent::setMeshAssetPath(const string& inMeshAssetPath)
{
	meshAssetPath = inMeshAssetPath;
	meshAsset = meshAssetPath.empty() ? nullptr : AssetLoader::get().loadSharedAsset<MeshAsset>(meshAssetPath);
	meshAssetHandle.reset();
	meshHandleReference.reset();
}

void MeshComponent::setLODLevel(const uint32 inLODLevel)
{
	if (lodLevel == inLODLevel)
	{
		return;
	}

	lodLevel = inLODLevel;
	meshAssetHandle.reset();
	meshHandleReference.reset();
}

void MeshComponent::setVisible(const bool inVisible)
{
	visible = inVisible;
}

void MeshComponent::setMaterialAssetPath(const uint32 sectionIndex, const string& materialAssetPath)
{
	if (materialAssetPaths.size() <= sectionIndex)
	{
		materialAssetPaths.resize(sectionIndex + 1);
		materialAssets.resize(sectionIndex + 1);
	}

	materialAssetPaths[sectionIndex] = materialAssetPath;
	materialAssets[sectionIndex] =
		materialAssetPath.empty() ? nullptr : AssetLoader::get().loadSharedAsset<MaterialAsset>(materialAssetPath);
}

void MeshComponent::setMaterialAssetPaths(const vector<string>& inMaterialAssetPaths)
{
	materialAssetPaths = inMaterialAssetPaths;
	reloadMaterialAssets();
}

void MeshComponent::requestMeshStreaming()
{
	if (meshAsset == nullptr)
	{
		meshAssetHandle.reset();
		return;
	}

	meshAssetHandle = MeshStreaming::get()->requestMesh(meshAsset, lodLevel);
}

void MeshComponent::clear()
{
	Component::clear();
	meshAssetPath.clear();
	meshAsset.reset();
	materialAssetPaths.clear();
	materialAssets.clear();
	materialHandleReferences.clear();
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
	xml.writePropertyArray(fileStream, "materialAssetPaths", materialAssetPaths);
}

void MeshComponent::readAssetProperty(const XMLKeyValueDocument& document)
{
	Component::readAssetProperty(document);

	XML& xml = XML::get();
	xml.readProperty(document, "deasset.meshAssetPath", meshAssetPath);
	xml.readProperty(document, "deasset.lodLevel", lodLevel);
	xml.readProperty(document, "deasset.visible", visible);
	xml.readPropertyArray(document, "deasset.materialAssetPaths", materialAssetPaths);

	meshAsset.reset();
	meshAssetHandle.reset();
	reloadMaterialAssets();
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
		materialHandleReferences.clear();
		meshHandleReference.reset();
		return;
	}

	const BridgeHandle entityHandle = getOwnerEntityHandle();
	assert(entityHandle != invalidBridgeHandle);
	refreshMaterialBridgeHandles();

	vector<BridgeHandle> materialHandles = {};
	materialHandles.reserve(materialHandleReferences.size());
	for (uint32 materialIndex = 0; materialIndex < static_cast<uint32>(materialHandleReferences.size()); ++materialIndex)
	{
		const MaterialBridge::HandleReference& materialHandleReference = materialHandleReferences[materialIndex];
		materialHandles.push_back(materialHandleReference.isValid()
			? materialHandleReference.getPackedHandle()
			: invalidBridgeHandle);
	}

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
			|| staticData->lodLevel != lodLevel
			|| staticData->materialHandles != materialHandles;
	}

	if (recreateMeshBridge)
	{
		meshHandleReference.reset();

		MeshBridge::ObjectDesc meshObjectDesc = {};
		meshObjectDesc.staticProperty.entityHandle = entityHandle;
		meshObjectDesc.staticProperty.meshAssetPath = meshAssetPath;
		meshObjectDesc.staticProperty.lodLevel = lodLevel;
		meshObjectDesc.staticProperty.meshAssetHandle = meshAssetHandle;
		meshObjectDesc.staticProperty.materialHandles = materialHandles;
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

void MeshComponent::refreshMaterialBridgeHandles()
{
	assert(materialAssets.size() == materialAssetPaths.size() && "[MeshComponent][Assert] reason=material_asset_storage_mismatch");
	materialHandleReferences.resize(materialAssetPaths.size());

	for (uint32 materialIndex = 0; materialIndex < static_cast<uint32>(materialAssetPaths.size()); ++materialIndex)
	{
		const string& materialAssetPath = materialAssetPaths[materialIndex];
		const shared_pointer<MaterialAsset>& materialAsset = materialAssets[materialIndex];
		MaterialBridge::HandleReference& materialHandleReference = materialHandleReferences[materialIndex];
		const bool needsMaterialBridge = !materialAssetPath.empty() && materialAsset != nullptr;
		if (!needsMaterialBridge)
		{
			materialHandleReference.reset();
			continue;
		}

		bool recreateMaterialBridge = !materialHandleReference.isValid();
		if (!recreateMaterialBridge)
		{
			const MaterialBridge::StaticData* staticData = MaterialBridge::get().getStaticData(materialHandleReference.getPackedHandle());
			recreateMaterialBridge = staticData == nullptr
				|| staticData->materialAssetPath != materialAssetPath
				|| staticData->materialAsset != materialAsset;
		}

		if (!recreateMaterialBridge)
		{
			continue;
		}

		materialHandleReference.reset();
		MaterialBridge::ObjectDesc materialObjectDesc = {};
		materialObjectDesc.staticProperty.materialAssetPath = materialAssetPath;
		materialObjectDesc.staticProperty.materialAsset = materialAsset;
		materialHandleReference = MaterialBridge::get().createMaterialHandle(materialObjectDesc);
		assert(materialHandleReference.isValid() && "[MeshComponent][Assert] reason=material_bridge_create_failed");
	}
}

void MeshComponent::reloadMaterialAssets()
{
	materialAssets.clear();
	materialAssets.resize(materialAssetPaths.size());
	for (uint32 materialIndex = 0; materialIndex < static_cast<uint32>(materialAssetPaths.size()); ++materialIndex)
	{
		const string& materialAssetPath = materialAssetPaths[materialIndex];
		if (materialAssetPath.empty())
		{
			continue;
		}

		materialAssets[materialIndex] = AssetLoader::get().loadSharedAsset<MaterialAsset>(materialAssetPath);
	}
}
