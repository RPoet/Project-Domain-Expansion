#pragma once

#include "Engine/Platform/PlatformDefine.h"

struct MeshAsset
{
	string name;
	using PositionData = float3;
	using NormalData = float3;
	using TexcoordData = float2;

	vector<PositionData> positionVertices;
	vector<NormalData> normalVertices;
	vector<TexcoordData> texcoordVertices;
	vector<uint32> indices;
	uint32 vertexCount = 0;
	uint32 indexCount = 0;
};

struct MeshObject
{
	string name;
	uint32 positionBufferIdentifier = 0;
	uint32 normalBufferIdentifier = 0;
	uint32 texcoordBufferIdentifier = 0;
	uint32 indexBufferIdentifier = 0;
};

