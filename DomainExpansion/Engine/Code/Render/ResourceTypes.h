#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/RenderTypes.h"

enum class ResourceObjectType : uint32
{
	unknown = 0,
	texture = 1,
	buffer = 2,
};

enum class BufferObjectMemoryType : uint32
{
	defaultHeap = 0,
	uploadHeap = 1,
	readbackHeap = 2,
};

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

