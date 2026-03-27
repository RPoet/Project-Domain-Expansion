#pragma once

#include <type_traits>

#include "Engine/Platform/PlatformDefine.h"

template <typename value_type>
inline std::enable_if_t<
	std::is_trivially_copyable_v<value_type>
	&& !std::is_pointer_v<value_type>
	&& !std::is_array_v<value_type>,
	OutputFileStream&>
operator<<(OutputFileStream& fileStream, const value_type& value)
{
	fileStream.write(reinterpret_cast<const char*>(&value), static_cast<stream_size>(sizeof(value_type)));
	return fileStream;
}

template <typename value_type>
inline std::enable_if_t<
	std::is_trivially_copyable_v<value_type>
	&& !std::is_pointer_v<value_type>
	&& !std::is_array_v<value_type>,
	InputFileStream&>
operator>>(InputFileStream& fileStream, value_type& outValue)
{
	fileStream.read(reinterpret_cast<char*>(&outValue), static_cast<stream_size>(sizeof(value_type)));
	return fileStream;
}

inline OutputFileStream& operator<<(OutputFileStream& fileStream, const string& text)
{
	const uint32 textLength = static_cast<uint32>(text.length());
	fileStream << textLength;
	if (textLength == 0)
	{
		return fileStream;
	}

	fileStream.write(text.data(), static_cast<stream_size>(textLength));
	return fileStream;
}

inline InputFileStream& operator>>(InputFileStream& fileStream, string& outText)
{
	outText.clear();

	uint32 textLength = 0;
	fileStream >> textLength;
	if (textLength == 0)
	{
		return fileStream;
	}

	outText.resize(textLength);
	fileStream.read(outText.data(), static_cast<stream_size>(textLength));
	return fileStream;
}
