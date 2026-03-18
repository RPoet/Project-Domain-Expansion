#pragma once

#include <math.h>

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

struct float4x4
{
	float value[16] = {};
};

inline float4x4 buildIdentityMatrix4x4()
{
	float4x4 matrix = {};
	matrix.value[0] = 1.0f;
	matrix.value[5] = 1.0f;
	matrix.value[10] = 1.0f;
	matrix.value[15] = 1.0f;
	return matrix;
}

inline float4x4 multiplyMatrix4x4(const float4x4& left, const float4x4& right)
{
	float4x4 result = {};
	for (int rowIndex = 0; rowIndex < 4; ++rowIndex)
	{
		for (int columnIndex = 0; columnIndex < 4; ++columnIndex)
		{
			float value = 0.0f;
			for (int elementIndex = 0; elementIndex < 4; ++elementIndex)
			{
				value += left.value[rowIndex * 4 + elementIndex] * right.value[elementIndex * 4 + columnIndex];
			}

			result.value[rowIndex * 4 + columnIndex] = value;
		}
	}

	return result;
}

inline float4x4 buildScaleMatrix4x4(const float3& scale)
{
	float4x4 matrix = {};
	matrix.value[0] = scale.x;
	matrix.value[5] = scale.y;
	matrix.value[10] = scale.z;
	matrix.value[15] = 1.0f;
	return matrix;
}

inline float4x4 buildRotationXMatrix4x4(const float angle)
{
	float4x4 matrix = buildIdentityMatrix4x4();
	const float cosineValue = cosf(angle);
	const float sineValue = sinf(angle);
	matrix.value[5] = cosineValue;
	matrix.value[6] = sineValue;
	matrix.value[9] = -sineValue;
	matrix.value[10] = cosineValue;
	return matrix;
}

inline float4x4 buildRotationYMatrix4x4(const float angle)
{
	float4x4 matrix = buildIdentityMatrix4x4();
	const float cosineValue = cosf(angle);
	const float sineValue = sinf(angle);
	matrix.value[0] = cosineValue;
	matrix.value[2] = -sineValue;
	matrix.value[8] = sineValue;
	matrix.value[10] = cosineValue;
	return matrix;
}

inline float4x4 buildRotationZMatrix4x4(const float angle)
{
	float4x4 matrix = buildIdentityMatrix4x4();
	const float cosineValue = cosf(angle);
	const float sineValue = sinf(angle);
	matrix.value[0] = cosineValue;
	matrix.value[1] = sineValue;
	matrix.value[4] = -sineValue;
	matrix.value[5] = cosineValue;
	return matrix;
}

inline float4x4 buildTranslationMatrix4x4(const float3& position)
{
	float4x4 matrix = buildIdentityMatrix4x4();
	matrix.value[12] = position.x;
	matrix.value[13] = position.y;
	matrix.value[14] = position.z;
	return matrix;
}

inline float4x4 buildWorldMatrix4x4(const float3& position, const float3& rotation, const float3& scale)
{
	const float4x4 scaleMatrix = buildScaleMatrix4x4(scale);
	const float4x4 rotationXMatrix = buildRotationXMatrix4x4(rotation.x);
	const float4x4 rotationYMatrix = buildRotationYMatrix4x4(rotation.y);
	const float4x4 rotationZMatrix = buildRotationZMatrix4x4(rotation.z);
	const float4x4 translationMatrix = buildTranslationMatrix4x4(position);
	return multiplyMatrix4x4(
		multiplyMatrix4x4(
			multiplyMatrix4x4(
				multiplyMatrix4x4(scaleMatrix, rotationXMatrix),
				rotationYMatrix),
			rotationZMatrix),
		translationMatrix);
}

inline float4x4 buildViewMatrix4x4(const float3& position, const float3& rotation)
{
	const float4x4 rotationXMatrix = buildRotationXMatrix4x4(-rotation.x);
	const float4x4 rotationYMatrix = buildRotationYMatrix4x4(-rotation.y);
	const float4x4 rotationZMatrix = buildRotationZMatrix4x4(-rotation.z);
	const float4x4 translationMatrix = buildTranslationMatrix4x4(position);
	return multiplyMatrix4x4(
		multiplyMatrix4x4(
			multiplyMatrix4x4(rotationXMatrix, rotationYMatrix),
			rotationZMatrix),
		translationMatrix);
}

inline float4x4 buildViewMatrix4x4(const float3& position)
{
	const float3 rotation = {};
	return buildViewMatrix4x4(position, rotation);
}

inline float4x4 buildProjectionMatrix4x4(
	const unsigned int viewportWidth,
	const unsigned int viewportHeight,
	const float fieldOfViewYDegrees,
	const float nearPlane,
	const float farPlane)
{
	float4x4 matrix = {};
	const float aspectRatio = viewportHeight > 0
		? static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight)
		: 1.0f;
	const float fieldOfViewY = fieldOfViewYDegrees * 3.1415926535f / 180.0f;
	const float tangentValue = tanf(fieldOfViewY * 0.5f);
	const float yScale = tangentValue > 0.0f ? 1.0f / tangentValue : 1.0f;
	const float xScale = aspectRatio > 0.0f ? yScale / aspectRatio : yScale;
	matrix.value[0] = xScale;
	matrix.value[5] = yScale;
	matrix.value[10] = farPlane / (farPlane - nearPlane);
	matrix.value[11] = 1.0f;
	matrix.value[14] = (-nearPlane * farPlane) / (farPlane - nearPlane);
	return matrix;
}

inline float4x4 buildProjectionMatrix4x4(const unsigned int viewportWidth, const unsigned int viewportHeight)
{
	return buildProjectionMatrix4x4(viewportWidth, viewportHeight, 60.0f, 0.1f, 100.0f);
}
