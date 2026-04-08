#pragma once

#include "Engine/Platform/PlatformDefine.h"

// TODO: Replace this helper with the project-owned string implementation once the engine string surface exists.
template <typename string_type>
inline string_type sliceString(
	const string_type& text,
	const typename string_type::size_type beginIndex,
	const typename string_type::size_type sliceLength = string_type::npos)
{
	assert(beginIndex <= text.length() && "[StringSlice][Assert] reason=begin_index_out_of_range");
	if (beginIndex == text.length())
	{
		return {};
	}

	if (sliceLength == string_type::npos)
	{
		return text.substr(beginIndex);
	}

	return text.substr(beginIndex, sliceLength);
}
