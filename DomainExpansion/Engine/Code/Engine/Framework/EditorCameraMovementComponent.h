#pragma once

#include "Engine/Framework/Component.h"

class CameraComponent;

class EditorCameraMovementComponent final : public Component
{
public:
	DECLARE_COMPONENT(EditorCameraMovementComponent);
	void clear() override;

	float getMovementSpeed() const
	{
		return movementSpeed;
	}

	void setMovementSpeed(float movementSpeed);
	void tick(float deltaTimeSeconds) override;
	void writeAssetProperty(OutputFileStream& fileStream) const override;
	void readAssetProperty(const XMLKeyValueDocument& document) override;

private:
	CameraComponent* getOwnerEditorCameraComponent();

	float movementSpeed = 4.0f;
};
