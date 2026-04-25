#pragma once

#include "Engine/Assets/Asset.h"
#include "Engine/Common/Singleton.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"

class MeshComponent;
class World;
class ThreadModule;
struct PreparedWorldLoadInput;
struct ParallelWorldLoadSharedState;

class AssetLoader final : public Singleton<AssetLoader>
{
public:
	enum class WorldLoadExecutionMode : uint32
	{
		automatic = 0,
		serial = 1,
		parallel = 2,
	};

	struct WorldLoadStats
	{
		WorldLoadExecutionMode requestedMode = WorldLoadExecutionMode::automatic;
		WorldLoadExecutionMode executedMode = WorldLoadExecutionMode::serial;
		string executionReason = {};
		uint32 workerThreadCount = 0;
		uint32 parseWorkerThreadUseCount = 0;
		uint32 entityWorkerThreadUseCount = 0;
		uint32 componentWorkerThreadUseCount = 0;
		uint32 entityReferenceCount = 0;
		uint32 componentReferenceCount = 0;
		uint32 entityJobCount = 0;
		uint32 componentJobCount = 0;
		uint32 assetResolveJobCount = 0;
		uint32 peakConcurrentParseJobCount = 0;
		uint32 peakConcurrentEntityJobCount = 0;
		uint32 peakConcurrentComponentJobCount = 0;
		uint32 assetResolveWorkerThreadUseCount = 0;
		uint32 peakConcurrentAssetResolveJobCount = 0;
		bool entityComponentOverlapObserved = false;
		float prepareMilliseconds = 0.0f;
		float executeMilliseconds = 0.0f;
		float entityReadWallMilliseconds = 0.0f;
		float entityReadAccumulatedMilliseconds = 0.0f;
		float entityMaterializeMilliseconds = 0.0f;
		float hierarchyMilliseconds = 0.0f;
		float componentReadWallMilliseconds = 0.0f;
		float componentReadAccumulatedMilliseconds = 0.0f;
		float componentAttachMilliseconds = 0.0f;
		float assetReferenceMilliseconds = 0.0f;
		float assetResolveAccumulatedMilliseconds = 0.0f;
		float runtimeObjectMilliseconds = 0.0f;
		float totalMilliseconds = 0.0f;
	};

	template <typename asset_type>
	asset_type loadAsset(const string& assetPathReference) const
	{
		shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
		const string assetPath = diskLoaderModule->resolveAssetPath(assetPathReference, DiskLoaderModule::AssetFileType::document);
		asset_type asset = {};
		asset.setAssetPath(assetPath);
		XMLKeyValueDocument& documentScratch = getThreadLocalAssetDocumentScratch();
		const XML::ParseCode parseCode = XML::get().readDocumentFile(assetPath, documentScratch);
		assert(parseCode == XML::ParseCode::succeeded && "[AssetLoader][Assert] reason=asset_document_read_failed");
		asset.readProperty(documentScratch);
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
	void setWorldLoadExecutionMode(WorldLoadExecutionMode executionMode);
	WorldLoadExecutionMode getWorldLoadExecutionMode() const;
	const WorldLoadStats& getLastWorldLoadStats() const;

private:
	static XMLKeyValueDocument& getThreadLocalAssetDocumentScratch();
	void resolveMeshComponentAssetReferences(const vector<MeshComponent*>& meshComponents) const;
	unique_pointer<World> loadWorldSerial(const PreparedWorldLoadInput& preparedInput, WorldLoadStats& inOutWorldLoadStats) const;
	unique_pointer<World> loadWorldParallel(
		PreparedWorldLoadInput&& preparedInput,
		ThreadModule& threadModule,
		WorldLoadStats& inOutWorldLoadStats) const;

	friend class Singleton<AssetLoader>;
	AssetLoader() = default;

	WorldLoadExecutionMode worldLoadExecutionMode = WorldLoadExecutionMode::automatic;
	mutable WorldLoadStats lastWorldLoadStats = {};
};

template <>
unique_pointer<World> AssetLoader::loadUniqueAsset<World>(const string& assetPathReference) const;
