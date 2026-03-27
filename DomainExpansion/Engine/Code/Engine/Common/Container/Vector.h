#pragma once

#include <type_traits>

#include "Engine/Common/FileStream.h"

template <typename value_type>
inline std::enable_if_t<std::is_trivially_copyable_v<value_type>, OutputFileStream&>
operator<<(OutputFileStream& fileStream, const vector<value_type>& values)
{
	const uint32 valueCount = static_cast<uint32>(values.size());
	fileStream << valueCount;
	if (valueCount == 0)
	{
		return fileStream;
	}

	fileStream.write(reinterpret_cast<const char*>(values.data()), static_cast<stream_size>(sizeof(value_type) * values.size()));
	return fileStream;
}

template <typename value_type>
inline std::enable_if_t<std::is_trivially_copyable_v<value_type>, InputFileStream&>
operator>>(InputFileStream& fileStream, vector<value_type>& values)
{
	values.clear();

	uint32 valueCount = 0;
	fileStream >> valueCount;
	if (valueCount == 0)
	{
		return fileStream;
	}

	values.resize(valueCount);
	fileStream.read(reinterpret_cast<char*>(values.data()), static_cast<stream_size>(sizeof(value_type) * values.size()));
	return fileStream;
}
