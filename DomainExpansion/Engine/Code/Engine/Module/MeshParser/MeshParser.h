#pragma once

#include "Engine/Assets/MeshAsset.h"
#include "Engine/Common/Singleton.h"
#include "Engine/Module/MeshParser/FbxMeshParserStub.h"
#include "Engine/Module/MeshParser/ObjMeshParser.h"

class World;

class MeshParser final : public Singleton<MeshParser>
{
public:
	enum class ImportCLIExecutionCode : int32
	{
		succeeded = 0,
		missingPath = -100,
		parseFailed = -101,
		fbxNotImplemented = -102,
		unsupportedExtension = -103,
		fileOpenFailed = -104,
	};

	bool parseFromFile(
		const string& meshFilePath,
		uint32 lodLevel,
		MeshAsset& outMeshAsset,
		string& outErrorText) const;
	bool importFromFile(
		const string& meshFilePath,
		uint32 lodLevel,
		const string& meshAssetPath,
		MeshAsset& outMeshAsset,
		string& outErrorText) const;
	bool importSceneFromFile(
		const string& meshFilePath,
		const string& meshAssetDirectoryPath,
		World& outWorld,
		uint32 parentEntityIndex,
		string& outErrorText) const;
	static void registerCLICommands();

private:
	friend class Singleton<MeshParser>;

	MeshParser();
	~MeshParser() = default;

	ObjMeshParser objMeshParser = {};
	FbxMeshParserStub fbxMeshParserStub = {};
};
