#pragma once

#include "Engine/Assets/MeshAsset.h"
#include "Engine/Platform/PlatformDefine.h"

class World;

class FbxMeshParserStub final
{
public:
	bool parse(
		const string& meshFilePath,
		uint32 lodLevel,
		MeshAsset& outMeshAsset,
		string& outErrorText) const;
	bool importEntityHierarchy(
		const string& meshFilePath,
		const string& meshAssetDirectoryPath,
		World& outWorld,
		uint32 parentEntityIndex,
		string& outErrorText) const;
};
