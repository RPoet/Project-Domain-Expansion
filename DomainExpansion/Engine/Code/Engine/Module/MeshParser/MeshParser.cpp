#include "Engine/Module/MeshParser/MeshParser.h"

#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/CLI/CLIModule.h"

static MeshParser& forceMeshParserSingleton = MeshParser::get();

static int32 getMeshParserImportExecutionCode(const string& errorText)
{
	if (errorText == "fbx_not_implemented")
	{
		return static_cast<int32>(MeshParser::ImportCLIExecutionCode::fbxNotImplemented);
	}

	if (errorText == "unsupported_extension")
	{
		return static_cast<int32>(MeshParser::ImportCLIExecutionCode::unsupportedExtension);
	}

	if (errorText == "file_open_failed")
	{
		return static_cast<int32>(MeshParser::ImportCLIExecutionCode::fileOpenFailed);
	}

	return static_cast<int32>(MeshParser::ImportCLIExecutionCode::parseFailed);
}

static int32 meshParserImportCLICommand(const string& parameter1, const string& parameter2, const string& parameter3)
{
	unused(parameter2);
	unused(parameter3);
	if (parameter1.empty())
	{
		return static_cast<int32>(MeshParser::ImportCLIExecutionCode::missingPath);
	}

	string meshFilePath = parameter1;
	string resolvedMeshFilePath = {};
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[MeshParser][Assert] reason=disk_loader_module_missing");
	if (diskLoaderModule->resolvePathFromResources(meshFilePath, resolvedMeshFilePath))
	{
		meshFilePath = resolvedMeshFilePath;
	}

	MeshAsset meshAsset = {};
	string errorText = {};
	if (!MeshParser::get().parseFromFile(meshFilePath, 0, meshAsset, errorText))
	{
		return getMeshParserImportExecutionCode(errorText);
	}

	return static_cast<int32>(MeshParser::ImportCLIExecutionCode::succeeded);
}

MeshParser::MeshParser()
{
	registerCLICommands();
}

void MeshParser::registerCLICommands()
{
	const bool importRegistered = CLIModule::registerCommand("MeshParser.import", meshParserImportCLICommand);
	assert(importRegistered && "[MeshParser][Assert] reason=mesh_parser_import_cli_register_failed");
	unused(importRegistered);
}

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
