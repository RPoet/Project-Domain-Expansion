#pragma once

#include "Engine/Module/Module.h"

class WindowsWindowObject;

enum class InputKeyState : uint32
{
	up = 0,
	pressed = 1,
	down = 2,
	released = 3,
};

enum class InputMouseButton : uint32
{
	left = 0,
	right = 1,
	middle = 2,
	x1 = 3,
	x2 = 4,
	count = 5,
};

class InputModule final : public StaticModule<InputModule>
{
public:
	static constexpr uint32 keyboardKeyCount = 256;
	static constexpr uint32 mouseButtonCount = static_cast<uint32>(InputMouseButton::count);

	InputModule()
		: StaticModule("InputModule")
	{
	}

	bool initialize(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	InputKeyState getKeyState(uint32 keyCode) const;
	InputKeyState getMouseButtonState(InputMouseButton mouseButton) const;
	int2 getMousePosition() const;
	int2 getMousePositionDelta() const;
	int2 getMouseScrollDelta() const;
	void handleNativeMessage(
		HandleWindow windowHandle,
		MessageIdentifier messageIdentifier,
		MessageFirstParameter firstParameter,
		MessageSecondParameter secondParameter);

private:
	void clearKeyboardKeyStates();
	void clearMouseStates();
	void updateMousePosition();

	WindowsWindowObject* windowObject = nullptr;
	InputKeyState keyStateStorage[keyboardKeyCount] = {};
	InputKeyState mouseButtonStateStorage[mouseButtonCount] = {};
	int2 mousePosition = {};
	int2 previousMousePosition = {};
	int2 mousePositionDelta = {};
	int2 mouseScrollDelta = {};
	int2 pendingMouseScrollDelta = {};
	bool hasMousePosition = false;
};
