#include "Engine/Module/MeshParser/FbxMeshParserStub.h"

bool FbxMeshParserStub::parse(
	const string& meshFilePath,
	const uint32 lodLevel,
	MeshAsset& outMeshAsset,
	string& outErrorText) const
{
	unused(meshFilePath);
	unused(lodLevel);
	outMeshAsset = {};
	outErrorText = "fbx_not_implemented";
	assert(false && "[MeshParser][Assert] reason=fbx_not_implemented");
}
