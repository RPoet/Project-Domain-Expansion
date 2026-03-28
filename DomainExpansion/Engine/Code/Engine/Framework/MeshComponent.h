#pragma once

#include "Engine/Assets/MeshAsset.h"
#include "Bridge/MeshBridge.h"
#include "Engine/Framework/Component.h"

struct MeshAssetHandle;

class MeshComponent final : public Component
{
public:
	DECLARE_COMPONENT(MeshComponent);
	void clear() override;

	string meshAssetPath = {};
	shared_pointer<MeshAsset> meshAsset = nullptr;
	uint32 lodLevel = 0;
	bool visible = true;
	BridgeHandle getMeshHandle() const;
	void generateMeshBridgeHandle();
	void initialize() override;
	void tick(float deltaTimeSeconds) override;

protected:
	void writeAssetProperty(OutputFileStream& fileStream) const override;
	void readAssetProperty(const XMLKeyValueDocument& document) override;

private:
	shared_pointer<MeshAssetHandle> meshAssetHandle = nullptr;
	MeshBridge::HandleReference meshHandleReference = {};
};
