#pragma once

#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <memory>
#include <memory_resource>
#include <new>
#include <limits>
#include <sstream>
#include <thread>
#include <string>
#include <string_view>
#include <system_error>
#include <atomic>
#include <type_traits>
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
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using wide_character = wchar_t;
using string = std::string;
using wstring = std::wstring;
using string_view = std::string_view;
using output_stream = std::ostream;
using error_stream = std::ostream;
using input_file_stream = std::ifstream;
using output_file_stream = std::ofstream;

using InputFileStream = input_file_stream;
using OutputFileStream = output_file_stream;

using string_input_stream = std::istringstream;
using string_output_stream = std::ostringstream;
using stream_position = std::streampos;
using stream_size = std::streamsize;
using error_code = std::error_code;
using memory_resource = std::pmr::memory_resource;
using unsynchronized_pool_resource = std::pmr::unsynchronized_pool_resource;
using align_val_t = std::align_val_t;
using nothrow_t = std::nothrow_t;
using bad_alloc = std::bad_alloc;
using steady_clock = std::chrono::steady_clock;
using duration_seconds = std::chrono::duration<double>;
using filesystem_path = std::filesystem::path;
using filesystem_directory_entry = std::filesystem::directory_entry;
using filesystem_directory_iterator = std::filesystem::directory_iterator;
using filesystem_directory_options = std::filesystem::directory_options;
using std::addressof;
using std::filesystem::create_directories;
using std::filesystem::current_path;
using std::filesystem::exists;
using std::filesystem::is_directory;
using std::filesystem::remove_all;
using std::getline;
using std::sort;
using std::terminate;
using std::tolower;
using std::to_string;
using std::transform;
using mutex = std::mutex;
using condition_variable = std::condition_variable;
template <typename type_name>
using atomic = std::atomic<type_name>;
using atomic_bool = std::atomic_bool;
template <typename mutex_type>
using lock_guard = std::lock_guard<mutex_type>;
template <typename mutex_type>
using unique_lock = std::unique_lock<mutex_type>;
template <typename type_name>
using deque = std::deque<type_name>;

inline output_stream& output = std::cout;
inline error_stream& error = std::cerr;
inline constexpr const nothrow_t& nothrowValue = std::nothrow;
inline constexpr char lineBreak = '\n';

inline void yieldCurrentThreadExecution()
{
	std::this_thread::yield();
}

inline void platformInitializeFailFastAssertBehavior();

[[noreturn]] inline void platformAssertFailFast(
	const char* expressionText,
	const char* filePath,
	const int32 lineNumber,
	const char* functionName)
{
	platformInitializeFailFastAssertBehavior();
	error_stream& failureStream = error;
	failureStream << "[Assert][Failure] expression=" << (expressionText != nullptr ? expressionText : "unknown")
				  << " file=" << (filePath != nullptr ? filePath : "unknown")
				  << " line=" << lineNumber
				  << " function=" << (functionName != nullptr ? functionName : "unknown")
				  << lineBreak;
	failureStream.flush();
	terminate();
}

#ifdef assert
#undef assert
#endif
#define assert(expression) ((expression) ? (void)0 : platformAssertFailFast(#expression, __FILE__, static_cast<int32>(__LINE__), __func__))

template <typename type_name>
constexpr decltype(auto) moveValue(type_name&& value) noexcept
{
	return std::move(value);
}

template <typename type_name>
constexpr decltype(auto) forwardValue(std::remove_reference_t<type_name>& value) noexcept
{
	return static_cast<type_name&&>(value);
}

template <typename type_name>
constexpr decltype(auto) forwardValue(std::remove_reference_t<type_name>&& value) noexcept
{
	return static_cast<type_name&&>(value);
}

template <typename value_type, typename... argument_types>
inline value_type* constructAt(value_type* address, argument_types&&... arguments)
{
	return std::construct_at(address, forwardValue<argument_types>(arguments)...);
}

template <typename value_type>
inline void destroyAt(value_type* address)
{
	std::destroy_at(address);
}

template <typename value_type>
inline void swapValue(value_type& left, value_type& right)
{
	value_type temporary = moveValue(left);
	left = moveValue(right);
	right = moveValue(temporary);
}

template <typename value_type>
using initializer_list = std::initializer_list<value_type>;

template <typename... type_names>
using void_t = std::void_t<type_names...>;

template <bool enabled, typename value_type = void>
using enable_if = std::enable_if_t<enabled, value_type>;

template <typename value_type>
using remove_reference_t = std::remove_reference_t<value_type>;

template <typename value_type>
using remove_cv_t = std::remove_cv_t<value_type>;

using true_type = std::true_type;
using false_type = std::false_type;

template <typename left_type, typename right_type>
inline constexpr bool is_same_v = std::is_same_v<left_type, right_type>;

template <typename value_type>
inline constexpr bool is_trivially_copyable = std::is_trivially_copyable_v<value_type>;

template <decltype(sizeof(0)) storage_size, decltype(sizeof(0)) storage_alignment>
using aligned_storage = std::aligned_storage_t<storage_size, storage_alignment>;

using max_align_storage = std::max_align_t;

#include "Engine/Common/Container/Vector.h"

template <typename signature>
using function = std::function<signature>;

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

inline memory_resource* getDefaultMemoryResource()
{
	return std::pmr::get_default_resource();
}

inline void tolower(string& text)
{
	transform(text.begin(), text.end(), text.begin(),
		[](const unsigned char character)
		{
			return static_cast<char>(tolower(character));
		});
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

#include "Engine/Common/PlatformComPointer.h"

template <typename type_name>
using com_pointer = platform_com_pointer<type_name>;

inline constexpr Bool boolTrue = static_cast<Bool>(1);
inline constexpr Bool boolFalse = static_cast<Bool>(0);
inline constexpr uint32 uint32MaxValue = (std::numeric_limits<uint32>::max)();
