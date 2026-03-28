#pragma once

#include "Bridge/CameraBridge.h"
#include "Engine/Framework/Component.h"

class CameraComponent final : public Component
{
public:
	DECLARE_COMPONENT(CameraComponent);
	void clear() override;

	bool editorCamera = false;
	bool primary = false;
	float fieldOfViewYDegrees = 60.0f;
	float nearPlane = 0.1f;
	float farPlane = 100.0f;

	BridgeHandle getCameraHandle() const;
	void generateCameraBridgeHandle();
	void initialize() override;
	void tick(float deltaTimeSeconds) override;

protected:
	void writeAssetProperty(OutputFileStream& fileStream) const override;
	void readAssetProperty(const XMLKeyValueDocument& document) override;

private:
	CameraBridge::HandleReference cameraHandleReference = {};
};
