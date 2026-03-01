#pragma once

#include <cstdint>

inline uint64_t roundUpToPowerOfTwo(uint64_t value)
{
	if (value <= 1)
	{
		return 1;
	}

	--value;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value |= value >> 32;
	return value + 1;
}
