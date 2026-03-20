#pragma once

#include "Engine/Framework/Component.h"

class CameraComponent;

class EditorCameraMovementComponent final : public Component
{
public:
	DECLARE_COMPONENT(EditorCameraMovementComponent);

	float getMovementSpeed() const
	{
		return movementSpeed;
	}

	void setMovementSpeed(float movementSpeed);
	void tick(float deltaTimeSeconds) override;

private:
	CameraComponent* getOwnerEditorCameraComponent();

	float movementSpeed = 4.0f;
};
