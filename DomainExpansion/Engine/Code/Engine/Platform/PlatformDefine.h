#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Engine/Platform/InplaceVector.h"
#include "Engine/Platform/Math.h"
#include "Engine/Platform/SIMDMath.h"

#ifndef unused
#define unused(x) (void)(x)
#endif

using int32 = std::int32_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using wide_character = wchar_t;
using string = std::string;
using wstring = std::wstring;
using output_stream = std::ostream;
using error_stream = std::ostream;
using input_file_stream = std::ifstream;
using output_file_stream = std::ofstream;
using string_input_stream = std::istringstream;
using stream_position = std::streampos;
using memory_resource = std::pmr::memory_resource;
using unsynchronized_pool_resource = std::pmr::unsynchronized_pool_resource;
using steady_clock = std::chrono::steady_clock;
using duration_seconds = std::chrono::duration<double>;
using filesystem_path = std::filesystem::path;
using filesystem_directory_entry = std::filesystem::directory_entry;
using filesystem_directory_iterator = std::filesystem::directory_iterator;
using filesystem_directory_options = std::filesystem::directory_options;

inline output_stream& output = std::cout;
inline error_stream& error = std::cerr;
inline constexpr char lineBreak = '\n';

template <typename signature>
using function = std::function<signature>;

template <typename type_name>
using vector = std::vector<type_name>;

template <typename left_type_name, typename right_type_name>
using pair = std::pair<left_type_name, right_type_name>;

template <typename type_name>
using unique_pointer = std::unique_ptr<type_name>;

template <typename type_name>
using shared_pointer = std::shared_ptr<type_name>;

template <typename type_name>
using pooled_vector = std::pmr::vector<type_name>;

template <typename... type_names>
using unordered_map = std::unordered_map<type_names...>;

template <typename type_name>
constexpr decltype(auto) moveValue(type_name&& value) noexcept
{
	return std::move(value);
}

inline memory_resource* getDefaultMemoryResource()
{
	return std::pmr::get_default_resource();
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

template <typename type_name>
using com_pointer = platform_com_pointer<type_name>;

inline constexpr Bool boolTrue = static_cast<Bool>(1);
inline constexpr Bool boolFalse = static_cast<Bool>(0);
inline constexpr uint32 uint32MaxValue = (std::numeric_limits<uint32>::max)();
