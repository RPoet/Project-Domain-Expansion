#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/ResourceTypes.h"

class FbxMeshParserStub final
{
public:
	bool parse(
		const string& meshFilePath,
		uint32 lodLevel,
		MeshAsset& outMeshAsset,
		string& outErrorText) const;
};
