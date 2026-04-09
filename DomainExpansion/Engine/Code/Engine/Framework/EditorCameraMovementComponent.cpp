#include "Engine/Framework/EditorCameraMovementComponent.h"

#include "Engine/Framework/CameraComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/World.h"
#include "Engine/Common/Math/ScalarMath.h"
#include "Engine/Module/Input/InputModule.h"
#include "Engine/Module/UI/ImGuiLayerModule.h"

#include <math.h>

static constexpr float editorCameraMovementMinSpeed = 0.5f;
static constexpr float editorCameraMovementMaxSpeed = 128.0f;
static constexpr float editorCameraMovementScrollStepMultiplier = 1.2f;
static constexpr float editorCameraLookSensitivityRadiansPerPixel = 0.005f;
static constexpr float editorCameraMaxPitchRadians = 1.55334306f;

static bool isInputKeyStateActive(const InputKeyState inputKeyState)
{
	return inputKeyState == InputKeyState::pressed || inputKeyState == InputKeyState::down;
}

static float clampEditorCameraMovementSpeed(const float movementSpeed)
{
	if (movementSpeed < editorCameraMovementMinSpeed)
	{
		return editorCameraMovementMinSpeed;
	}

	if (movementSpeed > editorCameraMovementMaxSpeed)
	{
		return editorCameraMovementMaxSpeed;
	}

	return movementSpeed;
}

static float clampEditorCameraPitchRadians(const float pitchRadians)
{
	if (pitchRadians < -editorCameraMaxPitchRadians)
	{
		return -editorCameraMaxPitchRadians;
	}

	if (pitchRadians > editorCameraMaxPitchRadians)
	{
		return editorCameraMaxPitchRadians;
	}

	return pitchRadians;
}

CameraComponent* EditorCameraMovementComponent::getOwnerEditorCameraComponent()
{
	World* ownerWorld = getOwnerWorld();
	Entity* ownerEntity = ownerWorld != nullptr ? ownerWorld->getEntityByIndex(getOwnerEntityIndex()) : nullptr;
	if (ownerWorld == nullptr || ownerEntity == nullptr)
	{
		return nullptr;
	}

	for (uint32 componentArrayIndex = 0; componentArrayIndex < ownerEntity->getComponentCount(); ++componentArrayIndex)
	{
		Component* component = ownerWorld->getComponentByIndex(ownerEntity->getComponentIndex(componentArrayIndex));
		if (component == nullptr || component->getComponentType() != CameraComponent::staticComponentType)
		{
			continue;
		}

		CameraComponent* cameraComponent = static_cast<CameraComponent*>(component);
		if (!cameraComponent->editorCamera)
		{
			continue;
		}

		return cameraComponent;
	}

	return nullptr;
}

void EditorCameraMovementComponent::clear()
{
	Component::clear();
	movementSpeed.reset();
}

void EditorCameraMovementComponent::setMovementSpeed(const float movementSpeed)
{
	this->movementSpeed = clampEditorCameraMovementSpeed(movementSpeed);
}

void EditorCameraMovementComponent::writeAssetProperty(OutputFileStream& fileStream) const
{
	Component::writeAssetProperty(fileStream);

	XML& xml = XML::get();
	xml.writeProperty(fileStream, movementSpeed);
}

void EditorCameraMovementComponent::readAssetProperty(const XMLKeyValueDocument& document)
{
	Component::readAssetProperty(document);

	float readMovementSpeed = movementSpeed;
	XML& xml = XML::get();
	if (!xml.readProperty(document, "deasset", readMovementSpeed))
	{
		return;
	}

	setMovementSpeed(readMovementSpeed);
}

void EditorCameraMovementComponent::tick(const float deltaTimeSeconds)
{
	shared_pointer<InputModule> inputModule = InputModule::get();
	CameraComponent* ownerCameraComponent = getOwnerEditorCameraComponent();
	if (inputModule == nullptr || ownerCameraComponent == nullptr)
	{
		return;
	}

	shared_pointer<ImGuiLayerModule> imGuiLayerModule = ImGuiLayerModule::get();
	if (imGuiLayerModule == nullptr || !imGuiLayerModule->isEditorInputReady())
	{
		return;
	}

	const bool textInputActive = imGuiLayerModule->wantsTextInput();
	const bool mouseCaptureActive = imGuiLayerModule->wantsMouseCapture();
	const bool lookActive = !textInputActive && !mouseCaptureActive && isInputKeyStateActive(inputModule->getMouseButtonState(InputMouseButton::right));
	if (!mouseCaptureActive)
	{
		const int32 scrollSteps = inputModule->getMouseScrollDelta().y / WHEEL_DELTA;
		if (scrollSteps > 0)
		{
			for (int32 scrollStepIndex = 0; scrollStepIndex < scrollSteps; ++scrollStepIndex)
			{
				movementSpeed *= editorCameraMovementScrollStepMultiplier;
			}
		}
		else if (scrollSteps < 0)
		{
			for (int32 scrollStepIndex = 0; scrollStepIndex > scrollSteps; --scrollStepIndex)
			{
				movementSpeed /= editorCameraMovementScrollStepMultiplier;
			}
		}

		movementSpeed = clampEditorCameraMovementSpeed(movementSpeed);
	}

	if (deltaTimeSeconds <= 0.0f || textInputActive)
	{
		return;
	}

	World* ownerWorld = getOwnerWorld();
	Entity* ownerEntity = ownerWorld != nullptr ? ownerWorld->getEntityByIndex(getOwnerEntityIndex()) : nullptr;
	PlaceableEntity* ownerPlaceableEntity = dynamic_cast<PlaceableEntity*>(ownerEntity);
	if (ownerPlaceableEntity == nullptr)
	{
		return;
	}

	bool transformChanged = false;
	if (lookActive)
	{
		const int2 mousePositionDelta = inputModule->getMousePositionDelta();
		if (mousePositionDelta.x != 0 || mousePositionDelta.y != 0)
		{
			ownerPlaceableEntity->transform.rotationYaw += static_cast<float>(mousePositionDelta.x) * editorCameraLookSensitivityRadiansPerPixel;
			ownerPlaceableEntity->transform.rotationPitch = clampEditorCameraPitchRadians(
				ownerPlaceableEntity->transform.rotationPitch
				+ static_cast<float>(mousePositionDelta.y) * editorCameraLookSensitivityRadiansPerPixel);
			transformChanged = true;
		}
	}
	// TO DO : Virtualize WASD Key mapping to forward, backward, right, left like things.
	// Input module will process and return this virtualized key mapping value.
	const float forwardInput = (isInputKeyStateActive(inputModule->getKeyState('W')) ? 1.0f : 0.0f) - (isInputKeyStateActive(inputModule->getKeyState('S')) ? 1.0f : 0.0f);
	const float rightInput = (isInputKeyStateActive(inputModule->getKeyState('D')) ? 1.0f : 0.0f) - (isInputKeyStateActive(inputModule->getKeyState('A')) ? 1.0f : 0.0f);
	if (forwardInput == 0.0f && rightInput == 0.0f)
	{
		return;
	}

	const float yaw = ownerPlaceableEntity->transform.rotationYaw;
	const float pitch = ownerPlaceableEntity->transform.rotationPitch;
	const float cosinePitch = cosf(pitch);
	const float3 forwardDirection = {
		.x = sinf(yaw) * cosinePitch,
		.y = -sinf(pitch),
		.z = cosf(yaw) * cosinePitch,
	};
	const float3 rightDirection = {
		.x = cosf(yaw),
		.y = 0.0f,
		.z = -sinf(yaw),
	};
	float3 movementDirection = {
		.x = forwardDirection.x * forwardInput + rightDirection.x * rightInput,
		.y = forwardDirection.y * forwardInput + rightDirection.y * rightInput,
		.z = forwardDirection.z * forwardInput + rightDirection.z * rightInput,
	};
	movementDirection = normalizeFloat3(movementDirection);
	if (getFloat3LengthSquared(movementDirection) > 0.0f)
	{
		const float movementDistance = movementSpeed * deltaTimeSeconds;
		ownerPlaceableEntity->transform.positionX += movementDirection.x * movementDistance;
		ownerPlaceableEntity->transform.positionY += movementDirection.y * movementDistance;
		ownerPlaceableEntity->transform.positionZ += movementDirection.z * movementDistance;
		transformChanged = true;
	}

	if (transformChanged)
	{
		ownerCameraComponent->generateCameraBridgeHandle();
	}
}
