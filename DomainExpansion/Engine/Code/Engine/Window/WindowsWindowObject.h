#pragma once

#include "Engine/Platform/PlatformDefine.h"

struct WindowCreateOptions
{
	wstring windowTitle = L"DomainExpansion";
	int32 initialClientWidth = 1280;
	int32 initialClientHeight = 720;
	bool startVisible = true;
	bool startBorderlessFullscreen = false;
};

struct WindowStateSnapshot
{
	LongInteger windowStyle = 0;
	LongInteger windowExtendedStyle = 0;
	WindowPlacement windowPlacement = {};
	bool hasSnapshot = false;
};

struct WindowEventCallbacks
{
	function<void(uint32, uint32)> onResize;
	function<void(bool)> onActivationChanged;
};

class WindowsWindowObject
{
public:
	WindowsWindowObject();
	~WindowsWindowObject();

	WindowsWindowObject(const WindowsWindowObject&) = delete;
	WindowsWindowObject& operator=(const WindowsWindowObject&) = delete;

	bool create(const WindowCreateOptions& options);
	void destroy();

	void show();
	void hide();
	bool pumpMessages();

	void setTitle(const wide_character* title);

	uint32 getClientWidth() const;
	uint32 getClientHeight() const;
	float getDpiScale() const;

	bool isActive() const;
	bool isWindowMinimized() const;
	bool isWindowMaximized() const;
	bool isResizing() const;
	bool isBorderlessFullscreen() const;

	void toggleBorderlessFullscreen();
	void setCursorVisible(bool visible);
	void setCursorCaptured(bool captured);

	void setEventCallbacks(WindowEventCallbacks callbacks);

	HandleInstance getWindowInstanceHandle() const;
	HandleWindow getWindowHandle() const;

private:
	static MessageResult CALLBACK staticWindowProcedure(
		HandleWindow windowHandle,
		MessageIdentifier messageIdentifier,
		MessageFirstParameter firstParameter,
		MessageSecondParameter secondParameter);

	MessageResult handleWindowMessage(
		HandleWindow windowHandle,
		MessageIdentifier messageIdentifier,
		MessageFirstParameter firstParameter,
		MessageSecondParameter secondParameter);

	void updateClientSizeFromWindow();
	void handleSizeMessage(
		MessageFirstParameter firstParameter,
		MessageSecondParameter secondParameter);
	void handleDpiChangedMessage(
		MessageFirstParameter firstParameter,
		MessageSecondParameter secondParameter);
	void enterBorderlessFullscreen();
	void exitBorderlessFullscreen();
	void updateCursorCaptureRegion();

	HandleInstance windowInstanceHandle = nullptr;
	HandleWindow windowHandle = nullptr;
	wstring windowClassName;
	wstring windowTitle;
	uint32 clientWidth = 0;
	uint32 clientHeight = 0;
	DotsPerInch windowDotsPerInch = 96;
	bool closeRequested = false;
	bool activeState = true;
	bool minimizedState = false;
	bool maximizedState = false;
	bool resizingState = false;
	bool borderlessFullscreenState = false;
	bool cursorVisibleState = true;
	bool cursorCapturedState = false;
	WindowStateSnapshot windowStateSnapshot;
	WindowEventCallbacks windowEventCallbacks = {};
};


