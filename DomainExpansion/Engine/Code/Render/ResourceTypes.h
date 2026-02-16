#pragma once

#include "Engine/Platform/PlatformDefine.h"

struct MeshAsset
{
	string name;
	uint32 vertexCount = 0;
	uint32 indexCount = 0;
};

struct MeshObject
{
	string name;
	uint32 vertexBufferIdentifier = 0;
	uint32 indexBufferIdentifier = 0;
};

