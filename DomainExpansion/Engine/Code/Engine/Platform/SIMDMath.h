#pragma once

// Currently these are not SIMD data but inner implementation of these structure would be SIMD soon.
// Not to change existing codes in elsewhere, pre-defined those SIMD data.
struct float2
{
	float x = 0.0f;
	float y = 0.0f;
};

struct float3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};
