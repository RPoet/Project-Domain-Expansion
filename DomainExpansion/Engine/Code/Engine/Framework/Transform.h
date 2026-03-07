#pragma once

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
