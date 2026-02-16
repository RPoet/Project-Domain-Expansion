#include "Engine/Platform/PlatformDefine.h"
#include "Engine/Window/WindowsWindowObject.h"

int WINAPI wWinMain(
	HandleInstance windowInstanceHandle,
	HandleInstance previousWindowInstanceHandle,
	WideStringPointer commandLine,
	int commandShow)
{
	unused(windowInstanceHandle);
	unused(previousWindowInstanceHandle);
	unused(commandLine);
	unused(commandShow);

	WindowCreateOptions windowCreateOptions = {};
	windowCreateOptions.windowTitle = L"DomainExpansion Engine";
	windowCreateOptions.initialClientWidth = 1600;
	windowCreateOptions.initialClientHeight = 900;
	windowCreateOptions.startVisible = true;
	windowCreateOptions.startBorderlessFullscreen = false;

	WindowsWindowObject windowsWindowObject;
	WindowEventCallbacks windowEventCallbacks = {};
	windowEventCallbacks.onResize = [](uint32 width, uint32 height)
	{
		output << "Window resized to " << width << "x" << height << lineBreak;
	};
	windowEventCallbacks.onActivationChanged = [](bool isActive)
	{
		output << "Window activation changed: " << (isActive ? "active" : "inactive") << lineBreak;
	};
	windowsWindowObject.setEventCallbacks(windowEventCallbacks);

	if (!windowsWindowObject.create(windowCreateOptions))
	{
		error << "Failed to create main window." << lineBreak;
		return -1;
	}

	while (windowsWindowObject.pumpMessages())
	{
		if (windowsWindowObject.isWindowMinimized())
		{
			Sleep(16);
			continue;
		}

		Sleep(1);
	}

	windowsWindowObject.destroy();
	return 0;
}


