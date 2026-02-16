#pragma once

struct ANativeWindow;

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
using HandleWindow = ANativeWindow*;
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

