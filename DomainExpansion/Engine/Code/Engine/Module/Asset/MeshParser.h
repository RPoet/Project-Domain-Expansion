#pragma once

#include "Engine/Module/Asset/FbxMeshParserStub.h"
#include "Engine/Module/Asset/ObjMeshParser.h"
#include "Engine/Module/Singleton.h"

class MeshParser final : public Singleton<MeshParser>
{
public:
	bool parseFromFile(
		const string& meshFilePath,
		uint32 lodLevel,
		MeshAsset& outMeshAsset,
		string& outErrorText) const;

private:
	friend class Singleton<MeshParser>;

	MeshParser() = default;
	~MeshParser() = default;

	ObjMeshParser objMeshParser = {};
	FbxMeshParserStub fbxMeshParserStub = {};
};
