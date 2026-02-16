#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

#ifndef unused
#define unused(x) (void)(x)
#endif

using int32 = std::int32_t;
using uint32 = std::uint32_t;
using wide_character = wchar_t;
using string = std::string;
using wstring = std::wstring;
using output_stream = std::ostream;
using error_stream = std::ostream;

inline output_stream& output = std::cout;
inline error_stream& error = std::cerr;
inline constexpr char lineBreak = '\n';

template <typename signature>
using function = std::function<signature>;

template <typename type_name>
constexpr decltype(auto) move(type_name&& value) noexcept
{
	return std::move(value);
}

#if defined(_WIN32)
#include "Engine/Platform/PlatformDefineWin32.h"
#elif defined(__ANDROID__)
#include "Engine/Platform/PlatformDefineAndroid.h"
#elif defined(__APPLE__)
#include "Engine/Platform/PlatformDefineApple.h"
#else
#error PlatformDefine.h requires a supported platform implementation.
#endif

inline constexpr Bool boolTrue = static_cast<Bool>(1);
inline constexpr Bool boolFalse = static_cast<Bool>(0);
