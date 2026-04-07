#pragma once

#include "Engine/Assets/Asset.h"
#include "Engine/Common/Singleton.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"

class Component;
class World;

class AssetLoader final : public Singleton<AssetLoader>
{
public:
	template <typename asset_type>
	asset_type loadAsset(const string& assetPathReference) const
	{
		TRACE_EVENT("asset", "AssetLoader::loadAsset");
		shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
		const string assetPath = diskLoaderModule->resolveAssetPath(assetPathReference, DiskLoaderModule::AssetFileType::document);
		asset_type asset = {};
		asset.setAssetPath(assetPath);
		asset.readProperty(XML::get().readDocumentFile(assetPath));
		return asset;
	}

	template <typename asset_type>
	shared_pointer<asset_type> loadSharedAsset(const string& assetPathReference) const
	{
		asset_type asset = loadAsset<asset_type>(assetPathReference);
		return shared_pointer<asset_type>(new asset_type(moveValue(asset)));
	}

	template <typename asset_type>
	unique_pointer<asset_type> loadUniqueAsset(const string& assetPathReference) const
	{
		asset_type asset = loadAsset<asset_type>(assetPathReference);
		return unique_pointer<asset_type>(new asset_type(moveValue(asset)));
	}

	void saveWorld(World& world) const;

private:
	friend class Singleton<AssetLoader>;
	AssetLoader() = default;
};

template <>
unique_pointer<World> AssetLoader::loadUniqueAsset<World>(const string& assetPathReference) const;
