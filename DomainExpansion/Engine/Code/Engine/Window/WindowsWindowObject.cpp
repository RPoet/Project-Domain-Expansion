#include "Engine/Window/WindowsWindowObject.h"

static constexpr uint32 baseDotsPerInch = 96;
static constexpr wide_character windowClassNameLiteral[] = L"DomainExpansionWindowsWindowClass";

static void setCursorVisibilityState(const bool visible)
{
	if (visible)
	{
		while (ShowCursor(boolTrue) < 0)
		{
		}
		return;
	}

	while (ShowCursor(boolFalse) >= 0)
	{
	}
}

static void enableProcessDpiAwareness()
{
	const Bool didEnablePerMonitorAware = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	if (!didEnablePerMonitorAware)
	{
		SetProcessDPIAware();
	}
}

static WindowRectangle calculateWindowRectangleForClientSize(
	const int32 clientWidth,
	const int32 clientHeight,
	const WindowStyle windowStyle,
	const WindowExtendedStyle windowExtendedStyle)
{
	WindowRectangle rectangle = { 0, 0, clientWidth, clientHeight };
	AdjustWindowRectEx(&rectangle, windowStyle, boolFalse, windowExtendedStyle);
	return rectangle;
}

WindowsWindowObject::WindowsWindowObject()
{
	windowClassName = windowClassNameLiteral;
}

WindowsWindowObject::~WindowsWindowObject()
{
	destroy();
}

bool WindowsWindowObject::create(const WindowCreateOptions& options)
{
	if (windowHandle != nullptr)
	{
		return false;
	}

	enableProcessDpiAwareness();

	closeRequested = false;
	borderlessFullscreenState = false;
	cursorCapturedState = false;
	cursorVisibleState = true;
	activeState = true;
	minimizedState = false;
	maximizedState = false;
	resizingState = false;
	windowStateSnapshot = {};

	windowTitle = options.windowTitle;
	windowInstanceHandle = GetModuleHandleW(nullptr);
	if (windowInstanceHandle == nullptr)
	{
		error << "Window creation failed: module handle is null." << lineBreak;
		return false;
	}

	WindowClassDefinition windowClassDefinition = {};
	windowClassDefinition.cbSize = sizeof(WindowClassDefinition);
	windowClassDefinition.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	windowClassDefinition.lpfnWndProc = &WindowsWindowObject::staticWindowProcedure;
	windowClassDefinition.hInstance = windowInstanceHandle;
	windowClassDefinition.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	windowClassDefinition.hbrBackground = reinterpret_cast<HandleBrush>(COLOR_WINDOW + 1);
	windowClassDefinition.lpszClassName = windowClassName.c_str();

	const Atom classRegistrationResult = RegisterClassExW(&windowClassDefinition);
	if (classRegistrationResult == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
	{
		error << "Window creation failed: RegisterClassExW failed." << lineBreak;
		return false;
	}

	const WindowStyle windowStyle = WS_OVERLAPPEDWINDOW;
	const WindowExtendedStyle windowExtendedStyle = WS_EX_APPWINDOW;
	const WindowRectangle windowRectangle = calculateWindowRectangleForClientSize(
		options.initialClientWidth,
		options.initialClientHeight,
		windowStyle,
		windowExtendedStyle);

	const int32 windowWidth = windowRectangle.right - windowRectangle.left;
	const int32 windowHeight = windowRectangle.bottom - windowRectangle.top;

	windowHandle = CreateWindowExW(
		windowExtendedStyle,
		windowClassName.c_str(),
		windowTitle.c_str(),
		windowStyle,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowWidth,
		windowHeight,
		nullptr,
		nullptr,
		windowInstanceHandle,
		this);

	if (windowHandle == nullptr)
	{
		error << "Window creation failed: CreateWindowExW returned null." << lineBreak;
		return false;
	}

	windowDotsPerInch = GetDpiForWindow(windowHandle);
	updateClientSizeFromWindow();

	if (options.startBorderlessFullscreen)
	{
		enterBorderlessFullscreen();
	}

	if (options.startVisible)
	{
		show();
	}

	return true;
}

void WindowsWindowObject::destroy()
{
	if (cursorCapturedState)
	{
		setCursorCaptured(false);
	}

	if (windowHandle != nullptr)
	{
		DestroyWindow(windowHandle);
		windowHandle = nullptr;
	}

	if (!windowClassName.empty() && windowInstanceHandle != nullptr)
	{
		UnregisterClassW(windowClassName.c_str(), windowInstanceHandle);
	}

	windowInstanceHandle = nullptr;
	closeRequested = true;
	activeState = false;
	minimizedState = false;
	maximizedState = false;
	resizingState = false;
	borderlessFullscreenState = false;
}

void WindowsWindowObject::show()
{
	if (windowHandle != nullptr)
	{
		ShowWindow(windowHandle, SW_SHOW);
		UpdateWindow(windowHandle);
	}
}

void WindowsWindowObject::hide()
{
	if (windowHandle != nullptr)
	{
		ShowWindow(windowHandle, SW_HIDE);
	}
}

bool WindowsWindowObject::pumpMessages()
{
	Message windowMessage = {};
	while (PeekMessageW(&windowMessage, nullptr, 0, 0, PM_REMOVE))
	{
		if (windowMessage.message == WM_QUIT)
		{
			closeRequested = true;
			return false;
		}

		TranslateMessage(&windowMessage);
		DispatchMessageW(&windowMessage);
	}

	return !closeRequested;
}

void WindowsWindowObject::setTitle(const wide_character* title)
{
	if (title == nullptr)
	{
		windowTitle.clear();
	}
	else
	{
		windowTitle = title;
	}

	if (windowHandle != nullptr)
	{
		SetWindowTextW(windowHandle, windowTitle.c_str());
	}
}

uint32 WindowsWindowObject::getClientWidth() const
{
	return clientWidth;
}

uint32 WindowsWindowObject::getClientHeight() const
{
	return clientHeight;
}

float WindowsWindowObject::getDpiScale() const
{
	return static_cast<float>(windowDotsPerInch) / static_cast<float>(baseDotsPerInch);
}

bool WindowsWindowObject::isActive() const
{
	return activeState;
}

bool WindowsWindowObject::isWindowMinimized() const
{
	return minimizedState;
}

bool WindowsWindowObject::isWindowMaximized() const
{
	return maximizedState;
}

bool WindowsWindowObject::isResizing() const
{
	return resizingState;
}

bool WindowsWindowObject::isBorderlessFullscreen() const
{
	return borderlessFullscreenState;
}

void WindowsWindowObject::toggleBorderlessFullscreen()
{
	if (windowHandle == nullptr)
	{
		return;
	}

	if (borderlessFullscreenState)
	{
		exitBorderlessFullscreen();
		return;
	}

	enterBorderlessFullscreen();
}

void WindowsWindowObject::setCursorVisible(const bool visible)
{
	if (cursorVisibleState == visible)
	{
		return;
	}

	setCursorVisibilityState(visible);
	cursorVisibleState = visible;

	if (!visible)
	{
		SetCursor(nullptr);
	}
}

void WindowsWindowObject::setCursorCaptured(const bool captured)
{
	if (windowHandle == nullptr)
	{
		cursorCapturedState = false;
		return;
	}

	if (!captured)
	{
		ClipCursor(nullptr);
		cursorCapturedState = false;
		return;
	}

	if (!activeState)
	{
		cursorCapturedState = false;
		return;
	}

	cursorCapturedState = true;
	updateCursorCaptureRegion();
}

void WindowsWindowObject::setEventCallbacks(WindowEventCallbacks callbacks)
{
	windowEventCallbacks = moveValue(callbacks);
}

HandleInstance WindowsWindowObject::getWindowInstanceHandle() const
{
	return windowInstanceHandle;
}

HandleWindow WindowsWindowObject::getWindowHandle() const
{
	return windowHandle;
}

MessageResult CALLBACK WindowsWindowObject::staticWindowProcedure(
	HandleWindow currentWindowHandle,
	MessageIdentifier messageIdentifier,
	MessageFirstParameter firstParameter,
	MessageSecondParameter secondParameter)
{
	if (messageIdentifier == WM_NCCREATE)
	{
		CreateStructure* windowCreateStructure = reinterpret_cast<CreateStructure*>(secondParameter);
		WindowsWindowObject* windowObject = static_cast<WindowsWindowObject*>(windowCreateStructure->lpCreateParams);
		SetWindowLongPtrW(currentWindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(windowObject));
		windowObject->windowHandle = currentWindowHandle;
	}

	WindowsWindowObject* windowObject = reinterpret_cast<WindowsWindowObject*>(GetWindowLongPtrW(currentWindowHandle, GWLP_USERDATA));

	if (windowObject != nullptr)
	{
		return windowObject->handleWindowMessage(
			currentWindowHandle,
			messageIdentifier,
			firstParameter,
			secondParameter);
	}

	return DefWindowProcW(currentWindowHandle, messageIdentifier, firstParameter, secondParameter);
}

MessageResult WindowsWindowObject::handleWindowMessage(
	HandleWindow currentWindowHandle,
	MessageIdentifier messageIdentifier,
	MessageFirstParameter firstParameter,
	MessageSecondParameter secondParameter)
{
	switch (messageIdentifier)
	{
	case WM_CLOSE:
		closeRequested = true;
		DestroyWindow(currentWindowHandle);
		return 0;

	case WM_DESTROY:
		closeRequested = true;
		PostQuitMessage(0);
		return 0;

	case WM_ENTERSIZEMOVE:
		resizingState = true;
		return 0;

	case WM_EXITSIZEMOVE:
		resizingState = false;
		updateClientSizeFromWindow();
		if (windowEventCallbacks.onResize && !minimizedState)
		{
			windowEventCallbacks.onResize(clientWidth, clientHeight);
		}
		if (cursorCapturedState)
		{
			updateCursorCaptureRegion();
		}
		return 0;

	case WM_SIZE:
		handleSizeMessage(firstParameter, secondParameter);
		return 0;

	case WM_DPICHANGED:
		handleDpiChangedMessage(firstParameter, secondParameter);
		return 0;

	case WM_ACTIVATEAPP:
		activeState = (firstParameter != 0);
		if (!activeState && cursorCapturedState)
		{
			setCursorCaptured(false);
		}
		if (windowEventCallbacks.onActivationChanged)
		{
			windowEventCallbacks.onActivationChanged(activeState);
		}
		return 0;

	default:
		break;
	}

	return DefWindowProcW(currentWindowHandle, messageIdentifier, firstParameter, secondParameter);
}

void WindowsWindowObject::updateClientSizeFromWindow()
{
	if (windowHandle == nullptr)
	{
		clientWidth = 0;
		clientHeight = 0;
		return;
	}

	WindowRectangle clientRectangle = {};
	GetClientRect(windowHandle, &clientRectangle);
	clientWidth = static_cast<uint32>(clientRectangle.right - clientRectangle.left);
	clientHeight = static_cast<uint32>(clientRectangle.bottom - clientRectangle.top);
}

void WindowsWindowObject::handleSizeMessage(
	MessageFirstParameter firstParameter,
	MessageSecondParameter secondParameter)
{
	const uint32 widthFromMessage = static_cast<uint32>(LOWORD(secondParameter));
	const uint32 heightFromMessage = static_cast<uint32>(HIWORD(secondParameter));

	clientWidth = widthFromMessage;
	clientHeight = heightFromMessage;

	if (firstParameter == SIZE_MINIMIZED)
	{
		minimizedState = true;
		maximizedState = false;
	}
	else if (firstParameter == SIZE_MAXIMIZED)
	{
		minimizedState = false;
		maximizedState = true;
	}
	else if (firstParameter == SIZE_RESTORED)
	{
		minimizedState = false;
		maximizedState = false;
	}

	if (!minimizedState && windowEventCallbacks.onResize)
	{
		windowEventCallbacks.onResize(clientWidth, clientHeight);
	}

	if (cursorCapturedState)
	{
		updateCursorCaptureRegion();
	}
}

void WindowsWindowObject::handleDpiChangedMessage(
	MessageFirstParameter firstParameter,
	MessageSecondParameter secondParameter)
{
	windowDotsPerInch = HIWORD(firstParameter);

	WindowRectangle* recommendedWindowRectangle = reinterpret_cast<WindowRectangle*>(secondParameter);
	if (recommendedWindowRectangle != nullptr)
	{
		SetWindowPos(
			windowHandle,
			nullptr,
			recommendedWindowRectangle->left,
			recommendedWindowRectangle->top,
			recommendedWindowRectangle->right - recommendedWindowRectangle->left,
			recommendedWindowRectangle->bottom - recommendedWindowRectangle->top,
			SWP_NOZORDER | SWP_NOACTIVATE);
	}
}

void WindowsWindowObject::enterBorderlessFullscreen()
{
	if (windowHandle == nullptr || borderlessFullscreenState)
	{
		return;
	}

	windowStateSnapshot.windowStyle = static_cast<LongInteger>(GetWindowLongPtrW(windowHandle, GWL_STYLE));
	windowStateSnapshot.windowExtendedStyle = static_cast<LongInteger>(GetWindowLongPtrW(windowHandle, GWL_EXSTYLE));
	windowStateSnapshot.windowPlacement = {};
	windowStateSnapshot.windowPlacement.length = sizeof(WindowPlacement);
	GetWindowPlacement(windowHandle, &windowStateSnapshot.windowPlacement);
	windowStateSnapshot.hasSnapshot = true;

	MonitorInfo monitorInformation = {};
	monitorInformation.cbSize = sizeof(MonitorInfo);
	const HandleMonitor monitorHandle = MonitorFromWindow(windowHandle, MONITOR_DEFAULTTONEAREST);
	GetMonitorInfoW(monitorHandle, &monitorInformation);

	SetWindowLongPtrW(windowHandle, GWL_STYLE, WS_POPUP | WS_VISIBLE);
	SetWindowPos(
		windowHandle,
		HWND_TOP,
		monitorInformation.rcMonitor.left,
		monitorInformation.rcMonitor.top,
		monitorInformation.rcMonitor.right - monitorInformation.rcMonitor.left,
		monitorInformation.rcMonitor.bottom - monitorInformation.rcMonitor.top,
		SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

	borderlessFullscreenState = true;
}

void WindowsWindowObject::exitBorderlessFullscreen()
{
	if (windowHandle == nullptr || !borderlessFullscreenState)
	{
		return;
	}

	if (!windowStateSnapshot.hasSnapshot)
	{
		return;
	}

	SetWindowLongPtrW(windowHandle, GWL_STYLE, windowStateSnapshot.windowStyle);
	SetWindowLongPtrW(windowHandle, GWL_EXSTYLE, windowStateSnapshot.windowExtendedStyle);
	SetWindowPlacement(windowHandle, &windowStateSnapshot.windowPlacement);
	SetWindowPos(
		windowHandle,
		nullptr,
		0,
		0,
		0,
		0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

	borderlessFullscreenState = false;
}

void WindowsWindowObject::updateCursorCaptureRegion()
{
	if (!cursorCapturedState || windowHandle == nullptr)
	{
		return;
	}

	WindowRectangle clientRectangle = {};
	GetClientRect(windowHandle, &clientRectangle);

	WindowPoint upperLeft = { clientRectangle.left, clientRectangle.top };
	WindowPoint lowerRight = { clientRectangle.right, clientRectangle.bottom };
	ClientToScreen(windowHandle, &upperLeft);
	ClientToScreen(windowHandle, &lowerRight);

	WindowRectangle screenRectangle = { upperLeft.x, upperLeft.y, lowerRight.x, lowerRight.y };
	ClipCursor(&screenRectangle);
}


