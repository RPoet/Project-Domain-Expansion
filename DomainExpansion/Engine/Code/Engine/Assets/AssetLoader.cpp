#include "Engine/Assets/AssetLoader.h"

#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/World.h"
#include "Engine/Module/Thread/ThreadModule.h"
#include "Engine/Module/Timer/Timer.h"

struct PreparedWorldLoadInput
{
	string worldAssetPath = {};
	XMLKeyValueDocument worldDocument = {};
	vector<string> entityAssetPaths = {};
	vector<string> componentAssetPaths = {};
};

struct ParallelWorldEntityLoadResult
{
	string absoluteAssetPath = {};
	unique_pointer<Entity> entity = nullptr;
};

struct ParallelWorldComponentLoadResult
{
	string absoluteOwnerEntityAssetPath = {};
	unique_pointer<Component> component = nullptr;
};

struct MeshAssetResolveBinding
{
	MeshComponent* meshComponent = nullptr;
	int32 meshAssetIndex = -1;
	vector<int32> materialAssetIndices = {};
};

struct MeshAssetResolvePlan
{
	vector<MeshAssetResolveBinding> componentBindings = {};
	vector<string> uniqueMeshAssetPaths = {};
	vector<string> uniqueMaterialAssetPaths = {};
};

struct ParallelWorldLoadSharedState
{
	PreparedWorldLoadInput preparedInput = {};
	vector<ParallelWorldEntityLoadResult> entityResults = {};
	vector<ParallelWorldComponentLoadResult> componentResults = {};
	unique_pointer<World> loadedWorld = nullptr;
	float entityMaterializeMilliseconds = 0.0f;
	float hierarchyMilliseconds = 0.0f;
	float componentAttachMilliseconds = 0.0f;
	mutex executionMetricsMutex = {};
	vector<uint64> parseWorkerThreadIds = {};
	vector<uint64> entityWorkerThreadIds = {};
	vector<uint64> componentWorkerThreadIds = {};
	vector<uint64> assetResolveWorkerThreadIds = {};
	atomic<uint32> activeParseJobCount = 0;
	atomic<uint32> activeEntityJobCount = 0;
	atomic<uint32> activeComponentJobCount = 0;
	atomic<uint32> activeAssetResolveJobCount = 0;
	atomic<uint32> peakConcurrentParseJobCount = 0;
	atomic<uint32> peakConcurrentEntityJobCount = 0;
	atomic<uint32> peakConcurrentComponentJobCount = 0;
	atomic<uint32> peakConcurrentAssetResolveJobCount = 0;
	atomic_bool entityComponentOverlapObserved = false;

	struct PhaseMetrics
	{
		atomic<uint64> firstStartMicroseconds = static_cast<uint64>(-1);
		atomic<uint64> lastEndMicroseconds = 0;
		atomic<uint64> accumulatedMicroseconds = 0;
	};

	PhaseMetrics entityReadMetrics = {};
	PhaseMetrics componentReadMetrics = {};
	PhaseMetrics assetResolveMetrics = {};
};

namespace
{
	XMLKeyValueDocument& getThreadLocalWorldLoadDocumentScratch()
	{
		static thread_local XMLKeyValueDocument documentScratch;
		return documentScratch;
	}

	inline constexpr uint64 parallelWorldTimestampUnset = static_cast<uint64>(-1);

	uint64 getParallelWorldTimestampMicroseconds()
	{
		return static_cast<uint64>(Timer::getCurrentTimeSeconds() * 1000000.0);
	}

	void updateParallelWorldMinimum(atomic<uint64>& minimumValue, const uint64 candidateValue)
	{
		uint64 currentMinimumValue = minimumValue.load();
		while (candidateValue < currentMinimumValue && !minimumValue.compare_exchange_weak(currentMinimumValue, candidateValue))
		{
		}
	}

	void updateParallelWorldMaximum(atomic<uint64>& maximumValue, const uint64 candidateValue)
	{
		uint64 currentMaximumValue = maximumValue.load();
		while (candidateValue > currentMaximumValue && !maximumValue.compare_exchange_weak(currentMaximumValue, candidateValue))
		{
		}
	}

	void recordParallelWorldPhaseSample(
		ParallelWorldLoadSharedState::PhaseMetrics& phaseMetrics,
		const uint64 startMicroseconds,
		const uint64 endMicroseconds)
	{
		updateParallelWorldMinimum(phaseMetrics.firstStartMicroseconds, startMicroseconds);
		updateParallelWorldMaximum(phaseMetrics.lastEndMicroseconds, endMicroseconds);
		phaseMetrics.accumulatedMicroseconds.fetch_add(endMicroseconds - startMicroseconds);
	}

	float resolveParallelWorldPhaseWallMilliseconds(const ParallelWorldLoadSharedState::PhaseMetrics& phaseMetrics)
	{
		const uint64 firstStartMicroseconds = phaseMetrics.firstStartMicroseconds.load();
		const uint64 lastEndMicroseconds = phaseMetrics.lastEndMicroseconds.load();
		if (firstStartMicroseconds == parallelWorldTimestampUnset || lastEndMicroseconds <= firstStartMicroseconds)
		{
			return 0.0f;
		}

		return static_cast<float>(lastEndMicroseconds - firstStartMicroseconds) / 1000.0f;
	}

	float resolveParallelWorldPhaseAccumulatedMilliseconds(const ParallelWorldLoadSharedState::PhaseMetrics& phaseMetrics)
	{
		return static_cast<float>(phaseMetrics.accumulatedMicroseconds.load()) / 1000.0f;
	}

	struct AccumulatingScopedTimer
	{
		explicit AccumulatingScopedTimer(float& inOutElapsedMilliseconds)
			: outElapsedMilliseconds(inOutElapsedMilliseconds)
		{
		}

		~AccumulatingScopedTimer()
		{
			outElapsedMilliseconds += stopwatch.getElapsedMilliseconds();
		}

		Stopwatch stopwatch = {};
		float& outElapsedMilliseconds;
	};

	void updateParallelWorldPeak(atomic<uint32>& peakValue, const uint32 candidateValue)
	{
		uint32 currentPeakValue = peakValue.load();
		while (currentPeakValue < candidateValue && !peakValue.compare_exchange_weak(currentPeakValue, candidateValue))
		{
		}
	}

	bool containsParallelWorldWorkerThreadId(const vector<uint64>& workerThreadIds, const uint64 nativeThreadId)
	{
		for (uint32 threadIndex = 0; threadIndex < static_cast<uint32>(workerThreadIds.size()); ++threadIndex)
		{
			if (workerThreadIds[threadIndex] == nativeThreadId)
			{
				return true;
			}
		}

		return false;
	}

	void recordParallelWorldWorkerThreadId(vector<uint64>& workerThreadIds, const uint64 nativeThreadId)
	{
		if (!containsParallelWorldWorkerThreadId(workerThreadIds, nativeThreadId))
		{
			workerThreadIds.push_back(nativeThreadId);
		}
	}

	struct ParallelWorldJobExecutionScope
	{
		ParallelWorldLoadSharedState& loadSharedState;
		bool entityJob = false;

		ParallelWorldJobExecutionScope(
			ParallelWorldLoadSharedState& inLoadSharedState,
			ThreadModule& threadModule,
			const bool inEntityJob)
			: loadSharedState(inLoadSharedState),
			  entityJob(inEntityJob)
		{
			const ThreadContext* currentThreadContext = threadModule.getCurrentContext();
			assert(currentThreadContext != nullptr && "[AssetLoader][Assert] reason=parallel_world_job_thread_context_missing");
			const uint64 nativeThreadId = currentThreadContext != nullptr ? currentThreadContext->nativeThreadId : 0;

			{
				lock_guard<mutex> metricsLock(loadSharedState.executionMetricsMutex);
				recordParallelWorldWorkerThreadId(loadSharedState.parseWorkerThreadIds, nativeThreadId);
				recordParallelWorldWorkerThreadId(
					entityJob ? loadSharedState.entityWorkerThreadIds : loadSharedState.componentWorkerThreadIds,
					nativeThreadId);
			}

			const uint32 activeParseJobCount = loadSharedState.activeParseJobCount.fetch_add(1) + 1;
			updateParallelWorldPeak(loadSharedState.peakConcurrentParseJobCount, activeParseJobCount);

			if (entityJob)
			{
				const uint32 activeEntityJobCount = loadSharedState.activeEntityJobCount.fetch_add(1) + 1;
				updateParallelWorldPeak(loadSharedState.peakConcurrentEntityJobCount, activeEntityJobCount);
				if (loadSharedState.activeComponentJobCount.load() > 0)
				{
					loadSharedState.entityComponentOverlapObserved.store(true);
				}
			}
			else
			{
				const uint32 activeComponentJobCount = loadSharedState.activeComponentJobCount.fetch_add(1) + 1;
				updateParallelWorldPeak(loadSharedState.peakConcurrentComponentJobCount, activeComponentJobCount);
				if (loadSharedState.activeEntityJobCount.load() > 0)
				{
					loadSharedState.entityComponentOverlapObserved.store(true);
				}
			}
		}

		~ParallelWorldJobExecutionScope()
		{
			const uint32 previousParseJobCount = loadSharedState.activeParseJobCount.fetch_sub(1);
			assert(previousParseJobCount > 0 && "[AssetLoader][Assert] reason=parallel_world_active_parse_job_count_underflow");
			if (entityJob)
			{
				const uint32 previousEntityJobCount = loadSharedState.activeEntityJobCount.fetch_sub(1);
				assert(previousEntityJobCount > 0 && "[AssetLoader][Assert] reason=parallel_world_active_entity_job_count_underflow");
				return;
			}

			const uint32 previousComponentJobCount = loadSharedState.activeComponentJobCount.fetch_sub(1);
			assert(previousComponentJobCount > 0 && "[AssetLoader][Assert] reason=parallel_world_active_component_job_count_underflow");
		}
	};

	struct ParallelWorldAssetResolveExecutionScope
	{
		ParallelWorldLoadSharedState& loadSharedState;

		ParallelWorldAssetResolveExecutionScope(
			ParallelWorldLoadSharedState& inLoadSharedState,
			ThreadModule& threadModule)
			: loadSharedState(inLoadSharedState)
		{
			const ThreadContext* currentThreadContext = threadModule.getCurrentContext();
			assert(currentThreadContext != nullptr && "[AssetLoader][Assert] reason=parallel_world_asset_resolve_thread_context_missing");
			const uint64 nativeThreadId = currentThreadContext != nullptr ? currentThreadContext->nativeThreadId : 0;

			{
				lock_guard<mutex> metricsLock(loadSharedState.executionMetricsMutex);
				recordParallelWorldWorkerThreadId(loadSharedState.assetResolveWorkerThreadIds, nativeThreadId);
			}

			const uint32 activeAssetResolveJobCount = loadSharedState.activeAssetResolveJobCount.fetch_add(1) + 1;
			updateParallelWorldPeak(loadSharedState.peakConcurrentAssetResolveJobCount, activeAssetResolveJobCount);
		}

		~ParallelWorldAssetResolveExecutionScope()
		{
			const uint32 previousAssetResolveJobCount = loadSharedState.activeAssetResolveJobCount.fetch_sub(1);
			assert(previousAssetResolveJobCount > 0 && "[AssetLoader][Assert] reason=parallel_world_active_asset_resolve_job_count_underflow");
		}
	};

	PreparedWorldLoadInput prepareWorldLoadInput(const string& worldAssetPath)
	{
		PreparedWorldLoadInput preparedInput = {
			.worldAssetPath = worldAssetPath,
		};
		preparedInput.worldDocument = XML::get().readDocumentFile(worldAssetPath);
		XML::get().readPropertyArray(preparedInput.worldDocument, "deasset.Entities", preparedInput.entityAssetPaths);
		XML::get().readPropertyArray(preparedInput.worldDocument, "deasset.Components", preparedInput.componentAssetPaths);
		return preparedInput;
	}

	uint32 computeParallelWorldJobCount(const uint32 referenceCount, const uint32 workerThreadCount)
	{
		if (referenceCount == 0 || workerThreadCount == 0)
		{
			return 0;
		}

		const uint32 targetJobCount = workerThreadCount * 4;
		return referenceCount < targetJobCount ? referenceCount : targetJobCount;
	}

	pair<uint32, uint32> buildParallelWorldJobRange(
		const uint32 itemCount,
		const uint32 jobIndex,
		const uint32 jobCount)
	{
		assert(jobCount > 0 && "[AssetLoader][Assert] reason=parallel_job_count_zero");
		assert(jobIndex < jobCount && "[AssetLoader][Assert] reason=parallel_job_index_out_of_range");
		const uint32 beginIndex = (itemCount * jobIndex) / jobCount;
		const uint32 endIndex = (itemCount * (jobIndex + 1)) / jobCount;
		return { beginIndex, endIndex };
	}

	void parseParallelWorldEntityRange(
		const DiskLoaderModule& diskLoaderModule,
		const vector<string>& entityAssetPaths,
		vector<ParallelWorldEntityLoadResult>& entityResults,
		const uint32 beginIndex,
		const uint32 endIndex)
	{
		XMLKeyValueDocument& entityDocument = getThreadLocalWorldLoadDocumentScratch();
		for (uint32 entityReferenceIndex = beginIndex; entityReferenceIndex < endIndex; ++entityReferenceIndex)
		{
			const string entityAssetPath = diskLoaderModule.resolveAssetPath(entityAssetPaths[entityReferenceIndex], DiskLoaderModule::AssetFileType::document);
			const XML::ParseCode parseCode = XML::get().readDocumentFile(entityAssetPath, entityDocument);
			assert(parseCode == XML::ParseCode::succeeded && "[AssetLoader][Assert] reason=entity_document_read_failed");
			string_view entityTypeName = {};
			const bool foundEntityTypeName = entityDocument.tryGetValueView("deasset.@type", entityTypeName);
			assert(foundEntityTypeName && "[AssetLoader][Assert] reason=entity_document_type_missing");

			unique_pointer<Entity> entityObject = Entity::createByAssetTypeName(string(entityTypeName.data(), entityTypeName.length()));
			assert(entityObject != nullptr && "[AssetLoader][Assert] reason=entity_create_failed");

			entityObject->setAssetPath(entityAssetPath);
			entityObject->readProperty(entityDocument);

			ParallelWorldEntityLoadResult& entityResult = entityResults[entityReferenceIndex];
			entityResult.absoluteAssetPath = diskLoaderModule.resolveAbsolutePathFromResources(entityAssetPath);
			entityResult.entity = moveValue(entityObject);
		}
	}

	void parseParallelWorldComponentRange(
		const DiskLoaderModule& diskLoaderModule,
		const vector<string>& componentAssetPaths,
		vector<ParallelWorldComponentLoadResult>& componentResults,
		const uint32 beginIndex,
		const uint32 endIndex)
	{
		XMLKeyValueDocument& componentDocument = getThreadLocalWorldLoadDocumentScratch();
		for (uint32 componentReferenceIndex = beginIndex; componentReferenceIndex < endIndex; ++componentReferenceIndex)
		{
			const string componentAssetPath = diskLoaderModule.resolveAssetPath(componentAssetPaths[componentReferenceIndex], DiskLoaderModule::AssetFileType::document);
			const XML::ParseCode parseCode = XML::get().readDocumentFile(componentAssetPath, componentDocument);
			assert(parseCode == XML::ParseCode::succeeded && "[AssetLoader][Assert] reason=component_document_read_failed");
			string_view componentTypeName = {};
			const bool foundComponentTypeName = componentDocument.tryGetValueView("deasset.@type", componentTypeName);
			assert(foundComponentTypeName && "[AssetLoader][Assert] reason=component_document_type_missing");

			unique_pointer<Component> componentObject = Component::createByAssetTypeName(string(componentTypeName.data(), componentTypeName.length()));
			assert(componentObject != nullptr && "[AssetLoader][Assert] reason=component_create_failed");

			componentObject->setAssetPath(componentAssetPath);
			componentObject->readProperty(componentDocument);

			ParallelWorldComponentLoadResult& componentResult = componentResults[componentReferenceIndex];
			componentResult.absoluteOwnerEntityAssetPath = diskLoaderModule.resolveAbsolutePathFromResources(componentObject->getOwnerEntityAssetPath());
			componentResult.component = moveValue(componentObject);
		}
	}

	vector<MeshComponent*> collectWorldMeshComponents(World& world)
	{
		vector<MeshComponent*> meshComponents = {};
		meshComponents.reserve(world.getComponentCount());
		for (uint32 componentIndex = 0; componentIndex < world.getComponentCount(); ++componentIndex)
		{
			MeshComponent* meshComponent = componentCast<MeshComponent>(world.getComponentByIndex(componentIndex));
			if (meshComponent == nullptr)
			{
				continue;
			}

			meshComponents.push_back(meshComponent);
		}

		return meshComponents;
	}

	MeshAssetResolvePlan buildMeshAssetResolvePlan(
		const DiskLoaderModule& diskLoaderModule,
		const vector<MeshComponent*>& meshComponents)
	{
		MeshAssetResolvePlan resolvePlan = {};
		unordered_map<string, uint32> meshAssetIndexByPath = {};
		unordered_map<string, uint32> materialAssetIndexByPath = {};

		for (uint32 componentIndex = 0; componentIndex < static_cast<uint32>(meshComponents.size()); ++componentIndex)
		{
			MeshComponent* meshComponent = meshComponents[componentIndex];
			if (meshComponent == nullptr)
			{
				continue;
			}

			MeshAssetResolveBinding binding = {
				.meshComponent = meshComponent,
			};

			if (!meshComponent->getMeshAssetPath().empty())
			{
				const string resolvedMeshAssetPath = diskLoaderModule.resolveAssetPath(meshComponent->getMeshAssetPath(), DiskLoaderModule::AssetFileType::document);
				const auto meshAssetIterator = meshAssetIndexByPath.find(resolvedMeshAssetPath);
				if (meshAssetIterator != meshAssetIndexByPath.end())
				{
					binding.meshAssetIndex = static_cast<int32>(meshAssetIterator->second);
				}
				else
				{
					const uint32 meshAssetIndex = static_cast<uint32>(resolvePlan.uniqueMeshAssetPaths.size());
					resolvePlan.uniqueMeshAssetPaths.push_back(resolvedMeshAssetPath);
					meshAssetIndexByPath[resolvedMeshAssetPath] = meshAssetIndex;
					binding.meshAssetIndex = static_cast<int32>(meshAssetIndex);
				}
			}

			const vector<string>& materialAssetPaths = meshComponent->getMaterialAssetPaths();
			const size_t materialAssetPathCount = materialAssetPaths.size();
			if (materialAssetPathCount > static_cast<size_t>(RawMeshData::invalidMaterialSlotIndex))
			{
				error << "[AssetLoader][Failure] reason=mesh_component_material_asset_path_count_invalid"
					  << " componentAssetPath=" << meshComponent->getAssetPath()
					  << " meshAssetPath=" << meshComponent->getMeshAssetPath()
					  << " materialAssetPathCount=" << materialAssetPathCount
					  << lineBreak;
				error.flush();
			}
			assert(materialAssetPathCount <= static_cast<size_t>(RawMeshData::invalidMaterialSlotIndex)
				&& "[AssetLoader][Assert] reason=mesh_component_material_asset_path_count_invalid");
			binding.materialAssetIndices.resize(materialAssetPaths.size(), -1);
			for (uint32 materialIndex = 0; materialIndex < static_cast<uint32>(materialAssetPaths.size()); ++materialIndex)
			{
				if (materialAssetPaths[materialIndex].empty())
				{
					continue;
				}

				const string resolvedMaterialAssetPath = diskLoaderModule.resolveAssetPath(materialAssetPaths[materialIndex], DiskLoaderModule::AssetFileType::document);
				const auto materialAssetIterator = materialAssetIndexByPath.find(resolvedMaterialAssetPath);
				if (materialAssetIterator != materialAssetIndexByPath.end())
				{
					binding.materialAssetIndices[materialIndex] = static_cast<int32>(materialAssetIterator->second);
					continue;
				}

				const uint32 materialAssetIndex = static_cast<uint32>(resolvePlan.uniqueMaterialAssetPaths.size());
				resolvePlan.uniqueMaterialAssetPaths.push_back(resolvedMaterialAssetPath);
				materialAssetIndexByPath[resolvedMaterialAssetPath] = materialAssetIndex;
				binding.materialAssetIndices[materialIndex] = static_cast<int32>(materialAssetIndex);
			}

			resolvePlan.componentBindings.push_back(moveValue(binding));
		}

		return resolvePlan;
	}

	template <typename asset_type>
	void loadSharedAssetRange(
		const AssetLoader& assetLoader,
		const vector<string>& assetPaths,
		vector<shared_pointer<asset_type>>& outLoadedAssets,
		const uint32 beginIndex,
		const uint32 endIndex)
	{
		for (uint32 assetIndex = beginIndex; assetIndex < endIndex; ++assetIndex)
		{
			outLoadedAssets[assetIndex] = assetLoader.loadSharedAsset<asset_type>(assetPaths[assetIndex]);
		}
	}

}

XMLKeyValueDocument& AssetLoader::getThreadLocalAssetDocumentScratch()
{
	static thread_local XMLKeyValueDocument documentScratch;
	return documentScratch;
}

void AssetLoader::resolveMeshComponentAssetReferences(const vector<MeshComponent*>& meshComponents) const
{
	if (meshComponents.empty())
	{
		return;
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[AssetLoader][Assert] reason=disk_loader_module_missing");

	const MeshAssetResolvePlan assetResolvePlan = buildMeshAssetResolvePlan(*diskLoaderModule, meshComponents);
	vector<shared_pointer<MeshAsset>> loadedMeshAssets = {};
	loadedMeshAssets.resize(assetResolvePlan.uniqueMeshAssetPaths.size());
	loadSharedAssetRange(*this, assetResolvePlan.uniqueMeshAssetPaths, loadedMeshAssets, 0, static_cast<uint32>(loadedMeshAssets.size()));

	vector<shared_pointer<MaterialAsset>> loadedMaterialAssets = {};
	loadedMaterialAssets.resize(assetResolvePlan.uniqueMaterialAssetPaths.size());
	loadSharedAssetRange(*this, assetResolvePlan.uniqueMaterialAssetPaths, loadedMaterialAssets, 0, static_cast<uint32>(loadedMaterialAssets.size()));

	for (uint32 bindingIndex = 0; bindingIndex < static_cast<uint32>(assetResolvePlan.componentBindings.size()); ++bindingIndex)
	{
		const MeshAssetResolveBinding& binding = assetResolvePlan.componentBindings[bindingIndex];
		assert(binding.meshComponent != nullptr && "[AssetLoader][Assert] reason=mesh_component_asset_resolve_binding_missing");

		shared_pointer<MeshAsset> loadedMeshAsset = nullptr;
		if (binding.meshAssetIndex >= 0)
		{
			loadedMeshAsset = loadedMeshAssets[static_cast<uint32>(binding.meshAssetIndex)];
		}

		vector<shared_pointer<MaterialAsset>> resolvedMaterialAssets = {};
		resolvedMaterialAssets.resize(binding.materialAssetIndices.size());
		for (uint32 materialIndex = 0; materialIndex < static_cast<uint32>(binding.materialAssetIndices.size()); ++materialIndex)
		{
			const int32 materialAssetIndex = binding.materialAssetIndices[materialIndex];
			if (materialAssetIndex < 0)
			{
				continue;
			}

			resolvedMaterialAssets[materialIndex] = loadedMaterialAssets[static_cast<uint32>(materialAssetIndex)];
		}

		binding.meshComponent->setLoadedAssetReferences(moveValue(loadedMeshAsset), moveValue(resolvedMaterialAssets));
	}
}

void AssetLoader::saveWorld(World& world) const
{
	assert(!world.getAssetPath().empty() && "[AssetLoader][Assert] reason=world_asset_path_missing");
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();

	for (uint32 entityIndex = 0; entityIndex < world.getEntityCount(); ++entityIndex)
	{
		Entity* entity = world.getEntityByIndex(entityIndex);
		assert(entity != nullptr && "[AssetLoader][Assert] reason=entity_missing");
		if (!diskLoaderModule->isWorldOwnedAssetPath(world.getAssetPath(), entity->getAssetPath()))
		{
			entity->setAssetPath(diskLoaderModule->buildGeneratedWorldEntityAssetPath(world.getAssetPath(), entityIndex));
		}
	}

	for (uint32 entityIndex = 0; entityIndex < world.getEntityCount(); ++entityIndex)
	{
		Entity* entity = world.getEntityByIndex(entityIndex);
		assert(entity != nullptr && "[AssetLoader][Assert] reason=entity_missing");
		for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
		{
			const uint32 componentIndex = entity->getComponentIndex(componentArrayIndex);
			Component* component = world.getComponentByIndex(componentIndex);
			assert(component != nullptr && "[AssetLoader][Assert] reason=component_missing");
			if (!diskLoaderModule->isWorldOwnedAssetPath(world.getAssetPath(), component->getAssetPath()))
			{
				component->setAssetPath(diskLoaderModule->buildGeneratedWorldComponentAssetPath(world.getAssetPath(), componentIndex));
			}
		}
	}

	for (uint32 entityIndex = 0; entityIndex < world.getEntityCount(); ++entityIndex)
	{
		Entity* entity = world.getEntityByIndex(entityIndex);
		assert(entity != nullptr && "[AssetLoader][Assert] reason=entity_missing");
		for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
		{
			const uint32 componentIndex = entity->getComponentIndex(componentArrayIndex);
			Component* component = world.getComponentByIndex(componentIndex);
			assert(component != nullptr && "[AssetLoader][Assert] reason=component_missing");
			component->setOwnerEntityAssetPath(entity->getAssetPath());
			assert(!component->getAssetPath().empty() && "[AssetLoader][Assert] reason=component_asset_path_missing");
			OutputFileStream componentFileStream = diskLoaderModule->openOutputFileStream(diskLoaderModule->resolveAbsolutePathFromResources(component->getAssetPath()), false, true);
			component->writeProperty(componentFileStream);
		}

		assert(!entity->getAssetPath().empty() && "[AssetLoader][Assert] reason=entity_asset_path_missing");
		OutputFileStream entityFileStream = diskLoaderModule->openOutputFileStream(diskLoaderModule->resolveAbsolutePathFromResources(entity->getAssetPath()), false, true);
		entity->writeProperty(entityFileStream);
	}

	OutputFileStream worldFileStream = diskLoaderModule->openOutputFileStream(diskLoaderModule->resolveAbsolutePathFromResources(world.getAssetPath()), false, true);
	world.writeProperty(worldFileStream);
}

void AssetLoader::setWorldLoadExecutionMode(const WorldLoadExecutionMode executionMode)
{
	worldLoadExecutionMode = executionMode;
}

AssetLoader::WorldLoadExecutionMode AssetLoader::getWorldLoadExecutionMode() const
{
	return worldLoadExecutionMode;
}

const AssetLoader::WorldLoadStats& AssetLoader::getLastWorldLoadStats() const
{
	return lastWorldLoadStats;
}

unique_pointer<World> AssetLoader::loadWorldSerial(
	const PreparedWorldLoadInput& preparedInput,
	WorldLoadStats& inOutWorldLoadStats) const
{
	unique_pointer<World> loadedWorld(new World());
	loadedWorld->setAssetPath(preparedInput.worldAssetPath);
	loadedWorld->readProperty(preparedInput.worldDocument);

	unordered_map<string, uint32> entityIndexByAssetPath = {};
	inOutWorldLoadStats.entityReferenceCount = static_cast<uint32>(preparedInput.entityAssetPaths.size());
	inOutWorldLoadStats.componentReferenceCount = static_cast<uint32>(preparedInput.componentAssetPaths.size());

	{
		PROFILE_SCOPE("startup", "AssetLoader::loadWorld.entities");
		XMLKeyValueDocument& entityDocument = getThreadLocalWorldLoadDocumentScratch();
		for (uint32 entityReferenceIndex = 0; entityReferenceIndex < static_cast<uint32>(preparedInput.entityAssetPaths.size()); ++entityReferenceIndex)
		{
			string entityAssetPath = {};
			string absoluteEntityAssetPath = {};
			unique_pointer<Entity> entityObject = nullptr;
			{
				AccumulatingScopedTimer entityReadTimer(inOutWorldLoadStats.entityReadWallMilliseconds);
				entityAssetPath = DiskLoaderModule::get()->resolveAssetPath(preparedInput.entityAssetPaths[entityReferenceIndex], DiskLoaderModule::AssetFileType::document);
				absoluteEntityAssetPath = DiskLoaderModule::get()->resolveAbsolutePathFromResources(entityAssetPath);
				const XML::ParseCode parseCode = XML::get().readDocumentFile(entityAssetPath, entityDocument);
				assert(parseCode == XML::ParseCode::succeeded && "[AssetLoader][Assert] reason=entity_document_read_failed");
				string_view entityTypeName = {};
				const bool foundEntityTypeName = entityDocument.tryGetValueView("deasset.@type", entityTypeName);
				assert(foundEntityTypeName && "[AssetLoader][Assert] reason=entity_document_type_missing");

				entityObject = Entity::createByAssetTypeName(string(entityTypeName.data(), entityTypeName.length()));
				assert(entityObject != nullptr && "[AssetLoader][Assert] reason=entity_create_failed");
				entityObject->setAssetPath(entityAssetPath);
				entityObject->readProperty(entityDocument);
			}

			{
				AccumulatingScopedTimer entityMaterializeTimer(inOutWorldLoadStats.entityMaterializeMilliseconds);
				const uint32 entityIndex = addressof(*loadedWorld)->addEntityObject(moveValue(entityObject), false);
				Entity* entity = loadedWorld->getEntityByIndex(entityIndex);
				assert(entity != nullptr && "[AssetLoader][Assert] reason=entity_missing");
				entityIndexByAssetPath[absoluteEntityAssetPath] = entityIndex;
			}
		}
	}
	inOutWorldLoadStats.entityReadAccumulatedMilliseconds = inOutWorldLoadStats.entityReadWallMilliseconds;

	{
		PROFILE_SCOPE("startup", "AssetLoader::loadWorld.hierarchy");
		ScopedTimer hierarchyTimer(inOutWorldLoadStats.hierarchyMilliseconds);
		for (uint32 entityIndex = 0; entityIndex < loadedWorld->getEntityCount(); ++entityIndex)
		{
			Entity* entity = loadedWorld->getEntityByIndex(entityIndex);
			assert(entity != nullptr && "[AssetLoader][Assert] reason=entity_missing");
			if (entity->getParentEntityAssetPath().empty())
			{
				continue;
			}

			const string absoluteParentEntityAssetPath = DiskLoaderModule::get()->resolveAbsolutePathFromResources(entity->getParentEntityAssetPath());
			const auto parentEntityIterator = entityIndexByAssetPath.find(absoluteParentEntityAssetPath);
			assert(parentEntityIterator != entityIndexByAssetPath.end() && "[AssetLoader][Assert] reason=parent_entity_asset_reference_missing");

			const bool addedChildEntity = loadedWorld->addChildEntity(parentEntityIterator->second, entityIndex);
			assert(addedChildEntity && "[AssetLoader][Assert] reason=child_entity_attach_failed");
		}
	}

	{
		PROFILE_SCOPE("startup", "AssetLoader::loadWorld.components");
		XMLKeyValueDocument& componentDocument = getThreadLocalWorldLoadDocumentScratch();
		for (uint32 componentReferenceIndex = 0; componentReferenceIndex < static_cast<uint32>(preparedInput.componentAssetPaths.size()); ++componentReferenceIndex)
		{
			unique_pointer<Component> component = nullptr;
			string absoluteOwnerEntityAssetPath = {};
			{
				AccumulatingScopedTimer componentReadTimer(inOutWorldLoadStats.componentReadWallMilliseconds);
				const string componentAssetPath = DiskLoaderModule::get()->resolveAssetPath(
					preparedInput.componentAssetPaths[componentReferenceIndex],
					DiskLoaderModule::AssetFileType::document);
				const XML::ParseCode parseCode = XML::get().readDocumentFile(componentAssetPath, componentDocument);
				assert(parseCode == XML::ParseCode::succeeded && "[AssetLoader][Assert] reason=component_document_read_failed");
				string_view componentTypeName = {};
				const bool foundComponentTypeName = componentDocument.tryGetValueView("deasset.@type", componentTypeName);
				assert(foundComponentTypeName && "[AssetLoader][Assert] reason=component_document_type_missing");

				component = Component::createByAssetTypeName(string(componentTypeName.data(), componentTypeName.length()));
				assert(component != nullptr && "[AssetLoader][Assert] reason=component_create_failed");

				component->setAssetPath(componentAssetPath);
				component->readProperty(componentDocument);
				absoluteOwnerEntityAssetPath = DiskLoaderModule::get()->resolveAbsolutePathFromResources(component->getOwnerEntityAssetPath());
			}

			const auto ownerEntityIterator = entityIndexByAssetPath.find(absoluteOwnerEntityAssetPath);
			assert(ownerEntityIterator != entityIndexByAssetPath.end() && "[AssetLoader][Assert] reason=component_owner_entity_asset_reference_missing");

			{
				AccumulatingScopedTimer componentAttachTimer(inOutWorldLoadStats.componentAttachMilliseconds);
				const bool attachedComponent = addressof(*loadedWorld)->attachComponent(ownerEntityIterator->second, moveValue(component), false);
				assert(attachedComponent && "[AssetLoader][Assert] reason=component_attach_failed");
			}
		}
	}
	inOutWorldLoadStats.componentReadAccumulatedMilliseconds = inOutWorldLoadStats.componentReadWallMilliseconds;

	{
		PROFILE_SCOPE("startup", "AssetLoader::loadWorld.asset_references");
		ScopedTimer assetReferenceTimer(inOutWorldLoadStats.assetReferenceMilliseconds);
		resolveMeshComponentAssetReferences(collectWorldMeshComponents(*loadedWorld));
	}
	inOutWorldLoadStats.assetResolveAccumulatedMilliseconds = inOutWorldLoadStats.assetReferenceMilliseconds;

	{
		PROFILE_SCOPE("startup", "AssetLoader::loadWorld.runtime_objects");
		ScopedTimer runtimeObjectTimer(inOutWorldLoadStats.runtimeObjectMilliseconds);
		addressof(*loadedWorld)->initializeRuntimeObjects();
	}

	return loadedWorld;
}

unique_pointer<World> AssetLoader::loadWorldParallel(
	PreparedWorldLoadInput&& preparedInput,
	ThreadModule& threadModule,
	WorldLoadStats& inOutWorldLoadStats) const
{
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[AssetLoader][Assert] reason=disk_loader_module_missing");

	const uint32 workerThreadCount = threadModule.getWorkerThreadCount();
	const uint32 entityJobCount = computeParallelWorldJobCount(static_cast<uint32>(preparedInput.entityAssetPaths.size()), workerThreadCount);
	const uint32 componentJobCount = computeParallelWorldJobCount(static_cast<uint32>(preparedInput.componentAssetPaths.size()), workerThreadCount);
	inOutWorldLoadStats.entityReferenceCount = static_cast<uint32>(preparedInput.entityAssetPaths.size());
	inOutWorldLoadStats.componentReferenceCount = static_cast<uint32>(preparedInput.componentAssetPaths.size());
	inOutWorldLoadStats.entityJobCount = entityJobCount;
	inOutWorldLoadStats.componentJobCount = componentJobCount;
	inOutWorldLoadStats.assetResolveJobCount = 0;

	shared_pointer<ParallelWorldLoadSharedState> loadSharedState(new ParallelWorldLoadSharedState());
	assert(loadSharedState != nullptr && "[AssetLoader][Assert] reason=parallel_world_load_state_allocate_failed");
	loadSharedState->preparedInput = moveValue(preparedInput);
	loadSharedState->entityResults.resize(loadSharedState->preparedInput.entityAssetPaths.size());
	loadSharedState->componentResults.resize(loadSharedState->preparedInput.componentAssetPaths.size());

	JobBatch parseBatch = {};
	parseBatch.initialize(entityJobCount + componentJobCount);
	for (uint32 entityJobIndex = 0; entityJobIndex < entityJobCount; ++entityJobIndex)
	{
		const pair<uint32, uint32> entityJobRange = buildParallelWorldJobRange(
			static_cast<uint32>(loadSharedState->preparedInput.entityAssetPaths.size()),
			entityJobIndex,
			entityJobCount);
		threadModule.submitJob({
			.debugName = "AssetLoader::loadWorld.entities.batch" + to_string(entityJobIndex),
			.queueType = JobQueueType::worker,
			.priority = JobPriority::high,
			.execute = [&threadModule, diskLoaderModule, loadSharedState, entityJobRange]()
			{
				PROFILE_SCOPE("startup", "AssetLoader::loadWorld.entities.batch");
				ParallelWorldJobExecutionScope jobExecutionScope(*loadSharedState, threadModule, true);
				const uint64 startMicroseconds = getParallelWorldTimestampMicroseconds();
				parseParallelWorldEntityRange(
					*diskLoaderModule,
					loadSharedState->preparedInput.entityAssetPaths,
					loadSharedState->entityResults,
					entityJobRange.first,
					entityJobRange.second);
				recordParallelWorldPhaseSample(
					loadSharedState->entityReadMetrics,
					startMicroseconds,
					getParallelWorldTimestampMicroseconds());
			},
			.dependency = {},
			.batchCounter = parseBatch.counter,
			.createCompletionHandle = false,
		});
	}

	for (uint32 componentJobIndex = 0; componentJobIndex < componentJobCount; ++componentJobIndex)
	{
		const pair<uint32, uint32> componentJobRange = buildParallelWorldJobRange(
			static_cast<uint32>(loadSharedState->preparedInput.componentAssetPaths.size()),
			componentJobIndex,
			componentJobCount);
		threadModule.submitJob({
			.debugName = "AssetLoader::loadWorld.components.batch" + to_string(componentJobIndex),
			.queueType = JobQueueType::worker,
			.priority = JobPriority::normal,
			.execute = [&threadModule, diskLoaderModule, loadSharedState, componentJobRange]()
			{
				PROFILE_SCOPE("startup", "AssetLoader::loadWorld.components.batch");
				ParallelWorldJobExecutionScope jobExecutionScope(*loadSharedState, threadModule, false);
				const uint64 startMicroseconds = getParallelWorldTimestampMicroseconds();
				parseParallelWorldComponentRange(
					*diskLoaderModule,
					loadSharedState->preparedInput.componentAssetPaths,
					loadSharedState->componentResults,
					componentJobRange.first,
					componentJobRange.second);
				recordParallelWorldPhaseSample(
					loadSharedState->componentReadMetrics,
					startMicroseconds,
					getParallelWorldTimestampMicroseconds());
			},
			.dependency = {},
			.batchCounter = parseBatch.counter,
			.createCompletionHandle = false,
		});
	}

	const JobHandle attachFinalizerHandle = threadModule.submitJob({
		.debugName = "AssetLoader::loadWorld.attach_finalize",
		.queueType = JobQueueType::mainThreadContinuation,
		.priority = JobPriority::high,
		.execute = [loadSharedState]()
		{
			PROFILE_SCOPE("startup", "AssetLoader::loadWorld.attach_finalize");
			unique_pointer<World> loadedWorld(new World());
			assert(loadedWorld != nullptr && "[AssetLoader][Assert] reason=world_allocate_failed");
			loadedWorld->setAssetPath(loadSharedState->preparedInput.worldAssetPath);
			loadedWorld->readProperty(loadSharedState->preparedInput.worldDocument);

			unordered_map<string, uint32> entityIndexByAssetPath = {};
			entityIndexByAssetPath.reserve(loadSharedState->entityResults.size());
			{
				PROFILE_SCOPE("startup", "AssetLoader::loadWorld.entities.materialize");
				ScopedTimer entityMaterializeTimer(loadSharedState->entityMaterializeMilliseconds);
				for (uint32 entityReferenceIndex = 0; entityReferenceIndex < static_cast<uint32>(loadSharedState->entityResults.size()); ++entityReferenceIndex)
				{
					ParallelWorldEntityLoadResult& entityResult = loadSharedState->entityResults[entityReferenceIndex];
					assert(entityResult.entity != nullptr && "[AssetLoader][Assert] reason=parallel_entity_result_missing");

					const uint32 entityIndex = addressof(*loadedWorld)->addEntityObject(moveValue(entityResult.entity), false);
					entityIndexByAssetPath[entityResult.absoluteAssetPath] = entityIndex;
				}
			}

			{
				PROFILE_SCOPE("startup", "AssetLoader::loadWorld.hierarchy");
				ScopedTimer hierarchyTimer(loadSharedState->hierarchyMilliseconds);
				for (uint32 entityIndex = 0; entityIndex < loadedWorld->getEntityCount(); ++entityIndex)
				{
					Entity* entity = loadedWorld->getEntityByIndex(entityIndex);
					assert(entity != nullptr && "[AssetLoader][Assert] reason=entity_missing");
					if (entity->getParentEntityAssetPath().empty())
					{
						continue;
					}

					const string absoluteParentEntityAssetPath = DiskLoaderModule::get()->resolveAbsolutePathFromResources(entity->getParentEntityAssetPath());
					const auto parentEntityIterator = entityIndexByAssetPath.find(absoluteParentEntityAssetPath);
					assert(parentEntityIterator != entityIndexByAssetPath.end() && "[AssetLoader][Assert] reason=parent_entity_asset_reference_missing");

					const bool addedChildEntity = loadedWorld->addChildEntity(parentEntityIterator->second, entityIndex);
					assert(addedChildEntity && "[AssetLoader][Assert] reason=child_entity_attach_failed");
				}
			}

			{
				PROFILE_SCOPE("startup", "AssetLoader::loadWorld.components.attach");
				ScopedTimer componentAttachTimer(loadSharedState->componentAttachMilliseconds);
				for (uint32 componentReferenceIndex = 0; componentReferenceIndex < static_cast<uint32>(loadSharedState->componentResults.size()); ++componentReferenceIndex)
				{
					ParallelWorldComponentLoadResult& componentResult = loadSharedState->componentResults[componentReferenceIndex];
					assert(componentResult.component != nullptr && "[AssetLoader][Assert] reason=parallel_component_result_missing");

					const auto ownerEntityIterator = entityIndexByAssetPath.find(componentResult.absoluteOwnerEntityAssetPath);
					assert(ownerEntityIterator != entityIndexByAssetPath.end() && "[AssetLoader][Assert] reason=component_owner_entity_asset_reference_missing");

					const bool attachedComponent = addressof(*loadedWorld)->attachComponent(ownerEntityIterator->second, moveValue(componentResult.component), false);
					assert(attachedComponent && "[AssetLoader][Assert] reason=component_attach_failed");
				}
			}

			loadSharedState->loadedWorld = moveValue(loadedWorld);
		},
		.dependency = parseBatch.getHandle(),
		.batchCounter = nullptr,
		.createCompletionHandle = true,
	});
	assert(attachFinalizerHandle.isValid() && "[AssetLoader][Assert] reason=parallel_world_attach_finalizer_submit_failed");

	threadModule.wait(attachFinalizerHandle);
	assert(loadSharedState->loadedWorld != nullptr && "[AssetLoader][Assert] reason=parallel_world_finalize_missing");

	World& loadedWorld = *loadSharedState->loadedWorld;
	{
		PROFILE_SCOPE("startup", "AssetLoader::loadWorld.asset_references");
		ScopedTimer assetReferenceTimer(inOutWorldLoadStats.assetReferenceMilliseconds);
		const vector<MeshComponent*> meshComponents = collectWorldMeshComponents(loadedWorld);
		const MeshAssetResolvePlan assetResolvePlan = buildMeshAssetResolvePlan(*diskLoaderModule, meshComponents);
		const uint32 meshAssetResolveJobCount = computeParallelWorldJobCount(static_cast<uint32>(assetResolvePlan.uniqueMeshAssetPaths.size()), workerThreadCount);
		const uint32 materialAssetResolveJobCount = computeParallelWorldJobCount(
			static_cast<uint32>(assetResolvePlan.uniqueMaterialAssetPaths.size()),
			workerThreadCount);
		inOutWorldLoadStats.assetResolveJobCount = meshAssetResolveJobCount + materialAssetResolveJobCount;

		vector<shared_pointer<MeshAsset>> loadedMeshAssets = {};
		loadedMeshAssets.resize(assetResolvePlan.uniqueMeshAssetPaths.size());
		vector<shared_pointer<MaterialAsset>> loadedMaterialAssets = {};
		loadedMaterialAssets.resize(assetResolvePlan.uniqueMaterialAssetPaths.size());

		if (inOutWorldLoadStats.assetResolveJobCount > 0)
		{
			JobBatch assetResolveBatch = {};
			assetResolveBatch.initialize(inOutWorldLoadStats.assetResolveJobCount);
			for (uint32 meshJobIndex = 0; meshJobIndex < meshAssetResolveJobCount; ++meshJobIndex)
			{
				const pair<uint32, uint32> meshJobRange = buildParallelWorldJobRange(
					static_cast<uint32>(assetResolvePlan.uniqueMeshAssetPaths.size()),
					meshJobIndex,
					meshAssetResolveJobCount);
				threadModule.submitJob({
					.debugName = "AssetLoader::loadWorld.asset_references.mesh.batch" + to_string(meshJobIndex),
					.queueType = JobQueueType::worker,
					.priority = JobPriority::normal,
					.execute = [this, &threadModule, loadSharedState, &assetResolvePlan, &loadedMeshAssets, meshJobRange]()
					{
						PROFILE_SCOPE("startup", "AssetLoader::loadWorld.asset_references.mesh.batch");
						ParallelWorldAssetResolveExecutionScope assetResolveExecutionScope(*loadSharedState, threadModule);
						const uint64 startMicroseconds = getParallelWorldTimestampMicroseconds();
						loadSharedAssetRange(*this, assetResolvePlan.uniqueMeshAssetPaths, loadedMeshAssets, meshJobRange.first, meshJobRange.second);
						recordParallelWorldPhaseSample(
							loadSharedState->assetResolveMetrics,
							startMicroseconds,
							getParallelWorldTimestampMicroseconds());
					},
					.dependency = {},
					.batchCounter = assetResolveBatch.counter,
					.createCompletionHandle = false,
				});
			}

			for (uint32 materialJobIndex = 0; materialJobIndex < materialAssetResolveJobCount; ++materialJobIndex)
			{
				const pair<uint32, uint32> materialJobRange = buildParallelWorldJobRange(
					static_cast<uint32>(assetResolvePlan.uniqueMaterialAssetPaths.size()),
					materialJobIndex,
					materialAssetResolveJobCount);
				threadModule.submitJob({
					.debugName = "AssetLoader::loadWorld.asset_references.material.batch" + to_string(materialJobIndex),
					.queueType = JobQueueType::worker,
					.priority = JobPriority::normal,
					.execute = [this, &threadModule, loadSharedState, &assetResolvePlan, &loadedMaterialAssets, materialJobRange]()
					{
						PROFILE_SCOPE("startup", "AssetLoader::loadWorld.asset_references.material.batch");
						ParallelWorldAssetResolveExecutionScope assetResolveExecutionScope(*loadSharedState, threadModule);
						const uint64 startMicroseconds = getParallelWorldTimestampMicroseconds();
						loadSharedAssetRange(*this, assetResolvePlan.uniqueMaterialAssetPaths, loadedMaterialAssets, materialJobRange.first, materialJobRange.second);
						recordParallelWorldPhaseSample(
							loadSharedState->assetResolveMetrics,
							startMicroseconds,
							getParallelWorldTimestampMicroseconds());
					},
					.dependency = {},
					.batchCounter = assetResolveBatch.counter,
					.createCompletionHandle = false,
				});
			}

			threadModule.wait(assetResolveBatch.getHandle());
		}

		for (uint32 bindingIndex = 0; bindingIndex < static_cast<uint32>(assetResolvePlan.componentBindings.size()); ++bindingIndex)
		{
			const MeshAssetResolveBinding& binding = assetResolvePlan.componentBindings[bindingIndex];
			assert(binding.meshComponent != nullptr && "[AssetLoader][Assert] reason=mesh_component_asset_resolve_binding_missing");

			shared_pointer<MeshAsset> loadedMeshAsset = nullptr;
			if (binding.meshAssetIndex >= 0)
			{
				loadedMeshAsset = loadedMeshAssets[static_cast<uint32>(binding.meshAssetIndex)];
			}

			vector<shared_pointer<MaterialAsset>> resolvedMaterialAssets = {};
			resolvedMaterialAssets.resize(binding.materialAssetIndices.size());
			for (uint32 materialIndex = 0; materialIndex < static_cast<uint32>(binding.materialAssetIndices.size()); ++materialIndex)
			{
				const int32 materialAssetIndex = binding.materialAssetIndices[materialIndex];
				if (materialAssetIndex < 0)
				{
					continue;
				}

				resolvedMaterialAssets[materialIndex] = loadedMaterialAssets[static_cast<uint32>(materialAssetIndex)];
			}

			binding.meshComponent->setLoadedAssetReferences(moveValue(loadedMeshAsset), moveValue(resolvedMaterialAssets));
		}
	}

	{
		PROFILE_SCOPE("startup", "AssetLoader::loadWorld.runtime_objects");
		ScopedTimer runtimeObjectTimer(inOutWorldLoadStats.runtimeObjectMilliseconds);
		loadedWorld.initializeRuntimeObjects();
	}

	inOutWorldLoadStats.entityReadWallMilliseconds = resolveParallelWorldPhaseWallMilliseconds(loadSharedState->entityReadMetrics);
	inOutWorldLoadStats.entityReadAccumulatedMilliseconds = resolveParallelWorldPhaseAccumulatedMilliseconds(loadSharedState->entityReadMetrics);
	inOutWorldLoadStats.entityMaterializeMilliseconds = loadSharedState->entityMaterializeMilliseconds;
	inOutWorldLoadStats.hierarchyMilliseconds = loadSharedState->hierarchyMilliseconds;
	inOutWorldLoadStats.componentReadWallMilliseconds = resolveParallelWorldPhaseWallMilliseconds(loadSharedState->componentReadMetrics);
	inOutWorldLoadStats.componentReadAccumulatedMilliseconds = resolveParallelWorldPhaseAccumulatedMilliseconds(loadSharedState->componentReadMetrics);
	inOutWorldLoadStats.componentAttachMilliseconds = loadSharedState->componentAttachMilliseconds;
	inOutWorldLoadStats.assetResolveAccumulatedMilliseconds = resolveParallelWorldPhaseAccumulatedMilliseconds(loadSharedState->assetResolveMetrics);

	{
		lock_guard<mutex> metricsLock(loadSharedState->executionMetricsMutex);
		inOutWorldLoadStats.parseWorkerThreadUseCount = static_cast<uint32>(loadSharedState->parseWorkerThreadIds.size());
		inOutWorldLoadStats.entityWorkerThreadUseCount = static_cast<uint32>(loadSharedState->entityWorkerThreadIds.size());
		inOutWorldLoadStats.componentWorkerThreadUseCount = static_cast<uint32>(loadSharedState->componentWorkerThreadIds.size());
		inOutWorldLoadStats.assetResolveWorkerThreadUseCount = static_cast<uint32>(loadSharedState->assetResolveWorkerThreadIds.size());
	}
	inOutWorldLoadStats.peakConcurrentParseJobCount = loadSharedState->peakConcurrentParseJobCount.load();
	inOutWorldLoadStats.peakConcurrentEntityJobCount = loadSharedState->peakConcurrentEntityJobCount.load();
	inOutWorldLoadStats.peakConcurrentComponentJobCount = loadSharedState->peakConcurrentComponentJobCount.load();
	inOutWorldLoadStats.peakConcurrentAssetResolveJobCount = loadSharedState->peakConcurrentAssetResolveJobCount.load();
	inOutWorldLoadStats.entityComponentOverlapObserved = loadSharedState->entityComponentOverlapObserved.load();
	return moveValue(loadSharedState->loadedWorld);
}

template <>
unique_pointer<World> AssetLoader::loadUniqueAsset<World>(const string& assetPathReference) const
{
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[AssetLoader][Assert] reason=disk_loader_module_missing");
	const string worldAssetPath = diskLoaderModule->resolveAssetPath(assetPathReference, DiskLoaderModule::AssetFileType::document);
	PROFILE_SCOPE_DETAIL("startup", "AssetLoader::loadWorld", worldAssetPath);

	PreparedWorldLoadInput preparedInput = {};
	WorldLoadStats worldLoadStats = {
		.requestedMode = worldLoadExecutionMode,
		.executedMode = WorldLoadExecutionMode::serial,
		.executionReason = "forced_serial",
		.workerThreadCount = 0,
	};

	unique_pointer<World> loadedWorld = nullptr;
	{
		ScopedTimer totalLoadTimer(worldLoadStats.totalMilliseconds);
		{
			ScopedTimer prepareTimer(worldLoadStats.prepareMilliseconds);
			preparedInput = prepareWorldLoadInput(worldAssetPath);
		}

		worldLoadStats.entityReferenceCount = static_cast<uint32>(preparedInput.entityAssetPaths.size());
		worldLoadStats.componentReferenceCount = static_cast<uint32>(preparedInput.componentAssetPaths.size());

		ThreadModule& threadModule = *ThreadModule::get();
		worldLoadStats.workerThreadCount = threadModule.getWorkerThreadCount();

		const bool workerThreadsAvailable = threadModule.getWorkerThreadCount() > 0;
		const bool requestedSerial = worldLoadExecutionMode == WorldLoadExecutionMode::serial;
		const bool requestedParallel = worldLoadExecutionMode == WorldLoadExecutionMode::parallel;
		const bool canUseParallelWorldLoad = workerThreadsAvailable;

		{
			ScopedTimer executeTimer(worldLoadStats.executeMilliseconds);
			if ((requestedParallel || worldLoadExecutionMode == WorldLoadExecutionMode::automatic) && canUseParallelWorldLoad)
			{
				worldLoadStats.executedMode = WorldLoadExecutionMode::parallel;
				worldLoadStats.executionReason = requestedParallel ? "forced_parallel" : "automatic_parallel";
				loadedWorld = loadWorldParallel(moveValue(preparedInput), threadModule, worldLoadStats);
			}
			else
			{
				if (requestedSerial)
				{
					worldLoadStats.executionReason = "forced_serial";
				}
				else if (!workerThreadsAvailable)
				{
					worldLoadStats.executionReason = requestedParallel ? "parallel_fallback_worker_thread_unavailable" : "automatic_worker_thread_unavailable";
				}

				loadedWorld = loadWorldSerial(preparedInput, worldLoadStats);
			}
		}

		if (loadedWorld == nullptr)
		{
			worldLoadStats.executionReason += "_load_failed";
		}
	}

	lastWorldLoadStats = worldLoadStats;
	return loadedWorld;
}
