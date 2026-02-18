#pragma once

#include "Engine/Platform/PlatformDefine.h"

struct MeshAsset
{
	string name;
	struct VertexData
	{
		float positionX = 0.0f;
		float positionY = 0.0f;
		float positionZ = 0.0f;
		float normalX = 0.0f;
		float normalY = 0.0f;
		float normalZ = 0.0f;
		float textureU = 0.0f;
		float textureV = 0.0f;
	};

	vector<VertexData> vertices;
	vector<uint32> indices;
	uint32 vertexCount = 0;
	uint32 indexCount = 0;
};

struct MeshObject
{
	string name;
	uint32 vertexBufferIdentifier = 0;
	uint32 indexBufferIdentifier = 0;
};

