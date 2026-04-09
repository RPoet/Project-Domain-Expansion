#pragma once

#include "Engine/Platform/SIMDMath.h"

#include <cmath>

static float fraction(const float value)
{
	return value - floorf(value);
}

static void HSVToRGB(
	const float hue,
	const float saturation,
	const float value,
	float& outRed,
	float& outGreen,
	float& outBlue)
{
	const float wrappedHue = fraction(hue) * 6.0f;
	const int32 hueSector = static_cast<int32>(wrappedHue);
	const float hueFraction = wrappedHue - static_cast<float>(hueSector);
	const float p = value * (1.0f - saturation);
	const float q = value * (1.0f - (saturation * hueFraction));
	const float t = value * (1.0f - (saturation * (1.0f - hueFraction)));

	switch (hueSector % 6)
	{
	case 0:
		outRed = value;
		outGreen = t;
		outBlue = p;
		return;
	case 1:
		outRed = q;
		outGreen = value;
		outBlue = p;
		return;
	case 2:
		outRed = p;
		outGreen = value;
		outBlue = t;
		return;
	case 3:
		outRed = p;
		outGreen = q;
		outBlue = value;
		return;
	case 4:
		outRed = t;
		outGreen = p;
		outBlue = value;
		return;
	default:
		outRed = value;
		outGreen = p;
		outBlue = q;
		return;
	}
}

static float getFloat3LengthSquared(const float3& value)
{
	return value.x * value.x + value.y * value.y + value.z * value.z;
}

static float3 normalizeFloat3(const float3& value)
{
	const float lengthSquared = getFloat3LengthSquared(value);
	if (lengthSquared <= 0.0f)
	{
		return {};
	}

	const float inverseLength = 1.0f / sqrtf(lengthSquared);
	return {
		.x = value.x * inverseLength,
		.y = value.y * inverseLength,
		.z = value.z * inverseLength,
	};
}
