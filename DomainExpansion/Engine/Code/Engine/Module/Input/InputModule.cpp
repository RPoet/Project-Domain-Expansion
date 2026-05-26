#include "Engine/Module/Input/InputModule.h"

#include "Engine/Framework/Framework.h"
#include "Engine/Window/WindowsWindowObject.h"


// TO DO : Change this as platform agnostic currently it is too tied with WIN.

static InputKeyState advanceInputKeyState(const InputKeyState currentState, const bool isCurrentlyDown)
{
	if (isCurrentlyDown)
	{
		return currentState == InputKeyState::pressed || currentState == InputKeyState::down
			? InputKeyState::down
			: InputKeyState::pressed;
	}

	return currentState == InputKeyState::pressed || currentState == InputKeyState::down
		? InputKeyState::released
		: InputKeyState::up;
}

static bool isVirtualKeyCurrentlyDown(const uint32 keyCode)
{
	return (GetAsyncKeyState(static_cast<int32>(keyCode)) & 0x8000) != 0;
}

static uint32 getMouseButtonIndex(const InputMouseButton mouseButton)
{
	return static_cast<uint32>(mouseButton);
}

static uint32 getMouseButtonVirtualKeyCode(const InputMouseButton mouseButton)
{
	switch (mouseButton)
	{
	case InputMouseButton::left:
		return VK_LBUTTON;
	case InputMouseButton::right:
		return VK_RBUTTON;
	case InputMouseButton::middle:
		return VK_MBUTTON;
	case InputMouseButton::x1:
		return VK_XBUTTON1;
	case InputMouseButton::x2:
		return VK_XBUTTON2;
	default:
		return 0;
	}
}

bool InputModule::initialize(Framework& framework)
{
	windowObject = framework.getWindowObject();
	clearKeyboardKeyStates();
	clearMouseStates();
	return true;
}

void InputModule::preUpdate()
{
	if (windowObject == nullptr || !windowObject->isActive())
	{
		clearKeyboardKeyStates();
		clearMouseStates();
		return;
	}

	updateMousePosition();
	mouseScrollDelta = pendingMouseScrollDelta;
	pendingMouseScrollDelta = {};

	for (uint32 keyCode = 0; keyCode < keyboardKeyCount; ++keyCode)
	{
		keyStateStorage[keyCode] = advanceInputKeyState(
			keyStateStorage[keyCode],
			isVirtualKeyCurrentlyDown(keyCode));
	}

	for (uint32 mouseButtonIndex = 0; mouseButtonIndex < mouseButtonCount; ++mouseButtonIndex)
	{
		const InputMouseButton mouseButton = static_cast<InputMouseButton>(mouseButtonIndex);
		mouseButtonStateStorage[mouseButtonIndex] = advanceInputKeyState(
			mouseButtonStateStorage[mouseButtonIndex],
			isVirtualKeyCurrentlyDown(getMouseButtonVirtualKeyCode(mouseButton)));
	}
}

void InputModule::postUpdate()
{
}

void InputModule::shutdown()
{
	clearKeyboardKeyStates();
	clearMouseStates();
	windowObject = nullptr;
}

InputKeyState InputModule::getKeyState(const uint32 keyCode) const
{
	assert(keyCode < keyboardKeyCount && "[InputModule][Assert] reason=invalid_key_code");
	return keyStateStorage[keyCode];
}

InputKeyState InputModule::getMouseButtonState(const InputMouseButton mouseButton) const
{
	const uint32 mouseButtonIndex = getMouseButtonIndex(mouseButton);
	assert(mouseButtonIndex < mouseButtonCount && "[InputModule][Assert] reason=invalid_mouse_button");
	return mouseButtonStateStorage[mouseButtonIndex];
}

int2 InputModule::getMousePosition() const
{
	return mousePosition;
}

int2 InputModule::getMousePositionDelta() const
{
	return mousePositionDelta;
}

int2 InputModule::getMouseScrollDelta() const
{
	return mouseScrollDelta;
}

void InputModule::handleNativeMessage(
	const HandleWindow windowHandle,
	const MessageIdentifier messageIdentifier,
	const MessageFirstParameter firstParameter,
	const MessageSecondParameter secondParameter)
{
	unused(secondParameter);

	if (windowObject == nullptr || windowHandle != windowObject->getWindowHandle())
	{
		return;
	}

	switch (messageIdentifier)
	{
	case WM_MOUSEWHEEL:
		pendingMouseScrollDelta.y += static_cast<int32>(GET_WHEEL_DELTA_WPARAM(firstParameter));
		break;
	case WM_MOUSEHWHEEL:
		pendingMouseScrollDelta.x += static_cast<int32>(GET_WHEEL_DELTA_WPARAM(firstParameter));
		break;
	default:
		break;
	}
}

void InputModule::clearKeyboardKeyStates()
{
	for (uint32 keyCode = 0; keyCode < keyboardKeyCount; ++keyCode)
	{
		keyStateStorage[keyCode] = InputKeyState::up;
	}
}

void InputModule::clearMouseStates()
{
	for (uint32 mouseButtonIndex = 0; mouseButtonIndex < mouseButtonCount; ++mouseButtonIndex)
	{
		mouseButtonStateStorage[mouseButtonIndex] = InputKeyState::up;
	}

	mousePosition = {};
	previousMousePosition = {};
	mousePositionDelta = {};
	mouseScrollDelta = {};
	pendingMouseScrollDelta = {};
	hasMousePosition = false;
}

void InputModule::updateMousePosition()
{
	if (windowObject == nullptr)
	{
		mousePosition = {};
		mousePositionDelta = {};
		previousMousePosition = {};
		hasMousePosition = false;
		return;
	}

	WindowPoint cursorPoint = {};
	if (!GetCursorPos(&cursorPoint) || !ScreenToClient(windowObject->getWindowHandle(), &cursorPoint))
	{
		mousePosition = {};
		mousePositionDelta = {};
		previousMousePosition = {};
		hasMousePosition = false;
		return;
	}

	const int2 nextMousePosition = {cursorPoint.x, cursorPoint.y};
	if (!hasMousePosition)
	{
		mousePosition = nextMousePosition;
		previousMousePosition = nextMousePosition;
		mousePositionDelta = {};
		hasMousePosition = true;
		return;
	}

	previousMousePosition = mousePosition;
	mousePosition = nextMousePosition;
	mousePositionDelta.x = mousePosition.x - previousMousePosition.x;
	mousePositionDelta.y = mousePosition.y - previousMousePosition.y;
}
