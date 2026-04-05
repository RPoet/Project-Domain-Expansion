#pragma once

#include "Engine/Platform/SIMDMath.h"

struct Transform
{
	float positionX = 0.0f;
	float positionY = 0.0f;
	float positionZ = 0.0f;
	float rotationPitch = 0.0f;
	float rotationYaw = 0.0f;
	float rotationRoll = 0.0f;
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	float scaleZ = 1.0f;
};

// TODO: Replace this TEMP_ helper path once World owns cached subtree world-transform updates.
inline float4x4 TEMP_buildTransformMatrix4x4(const Transform& transform)
{
	const float3 position = {
		.x = transform.positionX,
		.y = transform.positionY,
		.z = transform.positionZ,
	};
	const float3 rotation = {
		.x = transform.rotationPitch,
		.y = transform.rotationYaw,
		.z = transform.rotationRoll,
	};
	const float3 scale = {
		.x = transform.scaleX,
		.y = transform.scaleY,
		.z = transform.scaleZ,
	};
	return buildWorldMatrix4x4(position, rotation, scale);
}

// TODO: Replace this TEMP_ helper path once World owns cached subtree world-transform updates.
inline bool TEMP_buildTransformFromMatrix4x4(const float4x4& matrix, Transform& outTransform)
{
	const float row0LengthSquared = matrix.value[0] * matrix.value[0]
		+ matrix.value[1] * matrix.value[1]
		+ matrix.value[2] * matrix.value[2];
	const float row1LengthSquared = matrix.value[4] * matrix.value[4]
		+ matrix.value[5] * matrix.value[5]
		+ matrix.value[6] * matrix.value[6];
	const float row2LengthSquared = matrix.value[8] * matrix.value[8]
		+ matrix.value[9] * matrix.value[9]
		+ matrix.value[10] * matrix.value[10];

	outTransform = {};
	outTransform.positionX = matrix.value[12];
	outTransform.positionY = matrix.value[13];
	outTransform.positionZ = matrix.value[14];
	outTransform.scaleX = sqrtf(row0LengthSquared);
	outTransform.scaleY = sqrtf(row1LengthSquared);
	outTransform.scaleZ = sqrtf(row2LengthSquared);
	if (row0LengthSquared <= 0.000001f || row1LengthSquared <= 0.000001f || row2LengthSquared <= 0.000001f)
	{
		outTransform.rotationPitch = 0.0f;
		outTransform.rotationYaw = 0.0f;
		outTransform.rotationRoll = 0.0f;
		return true;
	}

	const float inverseScaleX = 1.0f / outTransform.scaleX;
	const float inverseScaleY = 1.0f / outTransform.scaleY;
	const float inverseScaleZ = 1.0f / outTransform.scaleZ;
	const float r00 = matrix.value[0] * inverseScaleX;
	const float r01 = matrix.value[1] * inverseScaleX;
	const float r02 = matrix.value[2] * inverseScaleX;
	const float r12 = matrix.value[6] * inverseScaleY;
	const float r22 = matrix.value[10] * inverseScaleZ;
	const float yawInput = r02 < -1.0f ? 1.0f : (r02 > 1.0f ? -1.0f : -r02);
	outTransform.rotationPitch = atan2f(r12, r22);
	outTransform.rotationYaw = asinf(yawInput);
	outTransform.rotationRoll = atan2f(r01, r00);
	return true;
}

inline bool operator==(const Transform& left, const Transform& right)
{
	return left.positionX == right.positionX
		&& left.positionY == right.positionY
		&& left.positionZ == right.positionZ
		&& left.rotationPitch == right.rotationPitch
		&& left.rotationYaw == right.rotationYaw
		&& left.rotationRoll == right.rotationRoll
		&& left.scaleX == right.scaleX
		&& left.scaleY == right.scaleY
		&& left.scaleZ == right.scaleZ;
}

inline bool operator!=(const Transform& left, const Transform& right)
{
	return !(left == right);
}
