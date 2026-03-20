#pragma once

#include "Bridge/CameraBridge.h"
#include "Engine/Framework/Component.h"

class CameraComponent final : public Component
{
public:
	DECLARE_COMPONENT(CameraComponent);

	bool editorCamera = false;
	bool primary = false;
	float fieldOfViewYDegrees = 60.0f;
	float nearPlane = 0.1f;
	float farPlane = 100.0f;

	BridgeHandle getCameraHandle() const;
	void generateCameraBridgeHandle();
	void tick(float deltaTimeSeconds) override;

protected:
	void initComponent() override;

private:
	CameraBridge::HandleReference cameraHandleReference = {};
};
