#pragma once

struct WindowRectangle
{
	int32 left = 0;
	int32 top = 0;
	int32 right = 0;
	int32 bottom = 0;
};

struct WindowPoint
{
	int32 x = 0;
	int32 y = 0;
};

struct WindowPlacement
{
	uint32 length = 0;
};

struct WindowClassDefinition
{
	uint32 size = 0;
};

struct CreateStructure
{
	void* userData = nullptr;
};

struct MonitorInfo
{
	uint32 size = 0;
	WindowRectangle monitorRectangle = {};
};

struct Message
{
	uint32 identifier = 0;
	uint32 firstParameter = 0;
	uint32 secondParameter = 0;
};

using HandleInstance = void*;
using HandleWindow = void*;
using MessageResult = int32;
using MessageIdentifier = uint32;
using MessageFirstParameter = uint32;
using MessageSecondParameter = uint32;
using WindowStyle = uint32;
using WindowExtendedStyle = uint32;
using DotsPerInch = uint32;
using LongInteger = int32;
using WideStringPointer = wide_character*;
using HandleMonitor = void*;
using Atom = uint32;
using Bool = bool;
using HandleBrush = void*;
using HandleEvent = void*;

inline void platformInitializeFailFastAssertBehavior()
{
}

template <typename type_name>
class platform_com_pointer
{
public:
	platform_com_pointer() = default;
	platform_com_pointer(decltype(nullptr))
	{
	}

	platform_com_pointer(type_name* rawPointer)
		: pointer(rawPointer)
	{
	}

	platform_com_pointer& operator=(type_name* rawPointer)
	{
		pointer = rawPointer;
		return *this;
	}

	platform_com_pointer& operator=(decltype(nullptr))
	{
		pointer = nullptr;
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
		pointer = nullptr;
		return &pointer;
	}

	void Reset()
	{
		pointer = nullptr;
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

private:
	type_name* pointer = nullptr;
};

