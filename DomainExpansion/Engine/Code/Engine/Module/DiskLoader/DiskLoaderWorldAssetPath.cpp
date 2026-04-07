#include "Engine/Module/DiskLoader/DiskLoaderModule.h"

filesystem_path DiskLoaderModule::buildWorldAssetDirectory(const string& worldAssetPath) const
{
	assert(!worldAssetPath.empty() && "[DiskLoaderModule][Assert] reason=world_asset_path_missing");
	const filesystem_path worldAssetFilePath(worldAssetPath);
	return (worldAssetFilePath.parent_path() / worldAssetFilePath.stem()).lexically_normal();
}

bool DiskLoaderModule::isWorldOwnedAssetPath(const string& worldAssetPath, const string& assetPath) const
{
	if (worldAssetPath.empty() || assetPath.empty())
	{
		return false;
	}

	return filesystem_path(assetPath).parent_path().lexically_normal() == buildWorldAssetDirectory(worldAssetPath);
}

string DiskLoaderModule::buildGeneratedWorldEntityAssetPath(const string& worldAssetPath, const uint32 entityIndex) const
{
	return (buildWorldAssetDirectory(worldAssetPath) / ("Entity" + to_string(entityIndex) + ".deasset")).lexically_normal().generic_string();
}

string DiskLoaderModule::buildGeneratedWorldComponentAssetPath(const string& worldAssetPath, const uint32 componentIndex) const
{
	const filesystem_path worldAssetFilePath(worldAssetPath);
	return (buildWorldAssetDirectory(worldAssetPath) / (worldAssetFilePath.stem().string() + "_Component" + to_string(componentIndex) + ".deasset")).lexically_normal().generic_string();
}
