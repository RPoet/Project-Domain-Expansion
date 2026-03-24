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
class platform_com_pointer
{
public:
	template <typename other_type_name>
	friend class platform_com_pointer;

	platform_com_pointer() = default;
	platform_com_pointer(decltype(nullptr))
	{
	}

	platform_com_pointer(type_name* rawPointer)
		: pointer(rawPointer)
	{
		internalAddRef();
	}

	platform_com_pointer(const platform_com_pointer& other)
		: pointer(other.pointer)
	{
		internalAddRef();
	}

	platform_com_pointer(platform_com_pointer&& other) noexcept
		: pointer(other.pointer)
	{
		other.pointer = nullptr;
	}

	~platform_com_pointer()
	{
		internalRelease();
	}

	platform_com_pointer& operator=(const platform_com_pointer& other)
	{
		if (this == addressof(other))
		{
			return *this;
		}

		assignRawPointer(other.pointer);
		return *this;
	}

	platform_com_pointer& operator=(platform_com_pointer&& other) noexcept
	{
		if (this == addressof(other))
		{
			return *this;
		}

		internalRelease();
		pointer = other.pointer;
		other.pointer = nullptr;
		return *this;
	}

	platform_com_pointer& operator=(type_name* rawPointer)
	{
		assignRawPointer(rawPointer);
		return *this;
	}

	platform_com_pointer& operator=(decltype(nullptr))
	{
		Reset();
		return *this;
	}

	type_name* Get() const
	{
		return pointer;
	}

	type_name** operator&()
	{
		return ReleaseAndGetAddressOf();
	}

	type_name** GetAddressOf()
	{
		return &pointer;
	}

	type_name* const* GetAddressOf() const
	{
		return &pointer;
	}

	type_name** ReleaseAndGetAddressOf()
	{
		internalRelease();
		return &pointer;
	}

	void Reset()
	{
		internalRelease();
	}

	type_name* operator->() const
	{
		return pointer;
	}

	explicit operator bool() const
	{
		return pointer != nullptr;
	}

	bool operator==(decltype(nullptr)) const
	{
		return pointer == nullptr;
	}

	bool operator!=(decltype(nullptr)) const
	{
		return pointer != nullptr;
	}

	template <typename other_type_name>
	HRESULT As(platform_com_pointer<other_type_name>* outPointer) const
	{
		assert(outPointer != nullptr && "[platform_com_pointer][Assert] reason=as_out_pointer_null");
		if (pointer == nullptr)
		{
			outPointer->Reset();
			return E_POINTER;
		}

		return pointer->QueryInterface(
			__uuidof(other_type_name),
			reinterpret_cast<void**>(outPointer->ReleaseAndGetAddressOf()));
	}

private:
	void internalAddRef()
	{
		if (pointer != nullptr)
		{
			pointer->AddRef();
		}
	}

	void internalRelease()
	{
		type_name* releasingPointer = pointer;
		pointer = nullptr;
		if (releasingPointer != nullptr)
		{
			releasingPointer->Release();
		}
	}

	void assignRawPointer(type_name* rawPointer)
	{
		if (pointer == rawPointer)
		{
			return;
		}

		if (rawPointer != nullptr)
		{
			rawPointer->AddRef();
		}

		internalRelease();
		pointer = rawPointer;
	}

	type_name* pointer = nullptr;
};

inline constexpr uint64 platformHashOffsetBasis = 14695981039346656037ull;
inline constexpr uint64 platformHashPrime = 1099511628211ull;

inline constexpr uint64 platformHashCombine(const uint64 currentHash, const uint64 value)
{
	return (currentHash ^ value) * platformHashPrime;
}

