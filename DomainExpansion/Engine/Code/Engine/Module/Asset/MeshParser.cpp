#include "Engine/Module/Asset/MeshParser.h"

bool MeshParser::parseFromFile(
	const string& meshFilePath,
	const uint32 lodLevel,
	MeshAsset& outMeshAsset,
	string& outErrorText) const
{
	outMeshAsset = {};
	outErrorText.clear();

	const filesystem_path meshPath(meshFilePath);
	const string extension = meshPath.extension().string();
	if (extension == ".obj" || extension == ".OBJ")
	{
		return objMeshParser.parse(meshFilePath, lodLevel, outMeshAsset, outErrorText);
	}

	if (extension == ".fbx" || extension == ".FBX")
	{
		return fbxMeshParserStub.parse(meshFilePath, lodLevel, outMeshAsset, outErrorText);
	}

	outErrorText = "unsupported_extension";
	error << "[MeshParser][Error] path=" << meshFilePath
		  << " reason=" << outErrorText << lineBreak;
	return false;
}
