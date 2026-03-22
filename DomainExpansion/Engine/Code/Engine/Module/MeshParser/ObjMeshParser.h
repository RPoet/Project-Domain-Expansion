#pragma once

#include "Engine/Assets/MeshAsset.h"
#include "Engine/Platform/PlatformDefine.h"

class ObjMeshParser final
{
public:
	bool parse(
		const string& meshFilePath,
		uint32 lodLevel,
		MeshAsset& outMeshAsset,
		string& outErrorText) const;
};
