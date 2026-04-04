#include "Engine/Module/MeshParser/MeshParser.h"

#include "Engine/Framework/World.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/CLI/CLIModule.h"

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

static int32 meshParserImportCLICommand(const vector<string>& arguments)
{
	if (arguments.size() != 1 || arguments[0].empty())
	{
		return static_cast<int32>(MeshParser::ImportCLIExecutionCode::missingPath);
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	const string meshAssetPath = diskLoaderModule->resolveAssetPath(arguments[0], DiskLoaderModule::AssetFileType::document);

	MeshAsset meshAsset = {};
	string errorText = {};
	if (!MeshParser::get().importFromFile(arguments[0], 0, meshAssetPath, meshAsset, errorText))
	{
		return getMeshParserImportExecutionCode(errorText);
	}

	return static_cast<int32>(MeshParser::ImportCLIExecutionCode::succeeded);
}

MeshParser::MeshParser()
{
}

void MeshParser::registerCLICommands()
{
	static bool cliCommandsRegistered = false;
	if (cliCommandsRegistered)
	{
		return;
	}

	const bool importRegistered = CLIModule::registerCommand("MeshParser.import", meshParserImportCLICommand);
	assert(importRegistered && "[MeshParser][Assert] reason=mesh_parser_import_cli_register_failed");
	unused(importRegistered);
	cliCommandsRegistered = true;
}

bool MeshParser::parseFromFile(
	const string& meshFilePath,
	const uint32 lodLevel,
	MeshAsset& outMeshAsset,
	string& outErrorText) const
{
	outMeshAsset = {};
	outErrorText.clear();

	string resolvedMeshFilePath = meshFilePath;
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	string absoluteMeshFilePath = {};
	if (diskLoaderModule->resolvePathFromResources(meshFilePath, absoluteMeshFilePath))
	{
		resolvedMeshFilePath = absoluteMeshFilePath;
	}

	const filesystem_path meshPath(resolvedMeshFilePath);
	const string extension = meshPath.extension().string();
	if (extension == ".obj" || extension == ".OBJ")
	{
		const bool parsed = objMeshParser.parse(resolvedMeshFilePath, lodLevel, outMeshAsset, outErrorText);
		if (parsed)
		{
			outMeshAsset.ensureSectionRanges();
		}

		return parsed;
	}

	if (extension == ".fbx" || extension == ".FBX")
	{
		const bool parsed = fbxMeshParserStub.parse(resolvedMeshFilePath, lodLevel, outMeshAsset, outErrorText);
		if (parsed)
		{
			outMeshAsset.ensureSectionRanges();
		}

		return parsed;
	}

	outErrorText = "unsupported_extension";
	assert(false && "[MeshParser][Assert] reason=unsupported_extension");
}

bool MeshParser::importFromFile(
	const string& meshFilePath,
	const uint32 lodLevel,
	const string& meshAssetPath,
	MeshAsset& outMeshAsset,
	string& outErrorText) const
{
	outMeshAsset = {};
	outErrorText.clear();
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();

	if (!parseFromFile(meshFilePath, lodLevel, outMeshAsset, outErrorText))
	{
		return false;
	}

	outMeshAsset.setName(filesystem_path(meshAssetPath).stem().string());
	outMeshAsset.setSource(meshFilePath);
	outMeshAsset.setAssetPath(meshAssetPath);

	const string meshAssetAbsolutePath = diskLoaderModule->resolveAbsolutePathFromResources(meshAssetPath);
	OutputFileStream meshAssetFileStream = diskLoaderModule->openOutputFileStream(meshAssetAbsolutePath, false, true);

	outMeshAsset.writeProperty(meshAssetFileStream);
	return true;
}

bool MeshParser::importSceneFromFile(
	const string& meshFilePath,
	const string& meshAssetDirectoryPath,
	World& outWorld,
	const uint32 parentEntityIndex,
	string& outErrorText) const
{
	outErrorText.clear();

	string resolvedMeshFilePath = meshFilePath;
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	string absoluteMeshFilePath = {};
	if (diskLoaderModule->resolvePathFromResources(meshFilePath, absoluteMeshFilePath))
	{
		resolvedMeshFilePath = absoluteMeshFilePath;
	}

	const filesystem_path meshPath(resolvedMeshFilePath);
	const string extension = meshPath.extension().string();
	if (extension == ".fbx" || extension == ".FBX")
	{
		return fbxMeshParserStub.importEntityHierarchy(
			resolvedMeshFilePath,
			meshAssetDirectoryPath,
			outWorld,
			parentEntityIndex,
			outErrorText);
	}

	outErrorText = "unsupported_extension";
	assert(false && "[MeshParser][Assert] reason=unsupported_scene_import_extension");
}
