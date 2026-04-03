#pragma once

#include "Engine/Assets/MaterialAsset.h"
#include "Engine/Assets/MeshAsset.h"
#include "Bridge/MaterialBridge.h"
#include "Bridge/MeshBridge.h"
#include "Engine/Framework/Component.h"

struct MeshAssetHandle;

class MeshComponent final : public Component
{
public:
	DECLARE_COMPONENT(MeshComponent);
	void clear() override;

	const string& getMeshAssetPath() const;
	shared_pointer<MeshAsset> getLoadedMeshAsset() const;
	uint32 getLODLevel() const;
	bool isVisible() const;
	bool hasLoadedMeshAsset() const;
	uint32 getMeshSectionCount() const;
	const vector<string>& getMaterialAssetPaths() const;
	shared_pointer<MaterialAsset> getMaterialAsset(uint32 sectionIndex) const;
	void setMeshAssetPath(const string& inMeshAssetPath);
	void setLODLevel(uint32 inLODLevel);
	void setVisible(bool inVisible);
	void setMaterialAssetPath(uint32 sectionIndex, const string& materialAssetPath);
	void setMaterialAssetPaths(const vector<string>& inMaterialAssetPaths);
	void requestMeshStreaming();
	BridgeHandle getMeshHandle() const;
	void generateMeshBridgeHandle();
	void initialize() override;
	void tick(float deltaTimeSeconds) override;

protected:
	void writeAssetProperty(OutputFileStream& fileStream) const override;
	void readAssetProperty(const XMLKeyValueDocument& document) override;

private:
	void refreshMaterialBridgeHandles();
	void reloadMaterialAssets();

	string meshAssetPath = {};
	shared_pointer<MeshAsset> meshAsset = nullptr;
	uint32 lodLevel = 0;
	bool visible = true;
	vector<string> materialAssetPaths = {};
	vector<shared_pointer<MaterialAsset>> materialAssets = {};
	vector<MaterialBridge::HandleReference> materialHandleReferences = {};
	shared_pointer<MeshAssetHandle> meshAssetHandle = nullptr;
	MeshBridge::HandleReference meshHandleReference = {};
};
