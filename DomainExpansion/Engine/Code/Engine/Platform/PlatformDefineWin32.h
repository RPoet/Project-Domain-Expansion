#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <crtdbg.h>
#include <stdlib.h>
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

inline void platformInitializeFailFastAssertBehavior()
{
	static const bool initialized = []() -> bool
	{
		SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
		_set_error_mode(_OUT_TO_STDERR);
		_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#if defined(_DEBUG)
		_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
		_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
		_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
		return true;
	}();

	unused(initialized);
}

template <typename type_name>
using platform_com_pointer = Microsoft::WRL::ComPtr<type_name>;

inline constexpr uint64 platformHashOffsetBasis = 14695981039346656037ull;
inline constexpr uint64 platformHashPrime = 1099511628211ull;

inline constexpr uint64 platformHashCombine(const uint64 currentHash, const uint64 value)
{
	return (currentHash ^ value) * platformHashPrime;
}

