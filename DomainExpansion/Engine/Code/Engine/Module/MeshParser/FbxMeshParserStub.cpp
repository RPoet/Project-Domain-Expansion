#include "Engine/Module/MeshParser/FbxMeshParserStub.h"

bool FbxMeshParserStub::parse(
	const string& meshFilePath,
	const uint32 lodLevel,
	MeshAsset& outMeshAsset,
	string& outErrorText) const
{
	unused(lodLevel);
	outMeshAsset = {};
	outErrorText = "fbx_not_implemented";
	error << "[MeshParser][Warn] path=" << meshFilePath
		  << " reason=" << outErrorText << lineBreak;
	return false;
}
