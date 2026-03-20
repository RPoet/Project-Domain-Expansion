#pragma once

#include "Bridge/MeshBridge.h"
#include "Engine/Framework/Component.h"

class MeshComponent final : public Component
{
public:
	DECLARE_COMPONENT(MeshComponent);

	string meshRelativePath = {};
	uint32 lodLevel = 0;
	bool visible = true;
	BridgeHandle getMeshHandle() const;
	void generateMeshBridgeHandle();
	void tick(float deltaTimeSeconds) override;

protected:
	void initComponent() override;

private:
	MeshBridge::HandleReference meshHandleReference = {};
};
