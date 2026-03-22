#pragma once

#include "Engine/Module/Asset/FbxMeshParserStub.h"
#include "Engine/Module/Asset/ObjMeshParser.h"
#include "Engine/Module/Singleton.h"

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

private:
	friend class Singleton<MeshParser>;

	MeshParser();
	~MeshParser() = default;
	void registerCLICommands();

	ObjMeshParser objMeshParser = {};
	FbxMeshParserStub fbxMeshParserStub = {};
};
