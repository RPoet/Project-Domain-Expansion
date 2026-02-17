#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <wrl/client.h>

using HandleInstance = HINSTANCE;
using HandleWindow = HWND;
using MessageResult = LRESULT;
using MessageIdentifier = UINT;
using MessageFirstParameter = WPARAM;
using MessageSecondParameter = LPARAM;
using WindowStyle = DWORD;
using WindowExtendedStyle = DWORD;
using DotsPerInch = UINT;
using LongInteger = LONG;
using WindowPlacement = WINDOWPLACEMENT;
using WideStringPointer = PWSTR;
using WindowRectangle = RECT;
using WindowPoint = POINT;
using WindowClassDefinition = WNDCLASSEXW;
using CreateStructure = CREATESTRUCTW;
using MonitorInfo = MONITORINFO;
using HandleMonitor = HMONITOR;
using Message = MSG;
using Atom = ATOM;
using Bool = BOOL;
using HandleBrush = HBRUSH;
using HandleEvent = HANDLE;

template <typename type_name>
using platform_com_pointer = Microsoft::WRL::ComPtr<type_name>;

