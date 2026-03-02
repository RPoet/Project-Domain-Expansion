#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/PipelineStateObject.h"

template <typename PlatformPipelineStateDescType, typename PlatformPipelineStateObjectType>
class PipelineStateManager
{
public:
	PlatformPipelineStateObjectType* find(
		const uint64 hashValue,
		const PlatformPipelineStateDescType& platformPipelineStateDesc)
	{
		auto foundBucket = pipelineStateObjectCacheByHash.find(hashValue);
		if (foundBucket == pipelineStateObjectCacheByHash.end())
		{
			return nullptr;
		}

		CacheBucket& cacheBucket = foundBucket->second;
		for (uint32 cacheIndex = 0; cacheIndex < static_cast<uint32>(cacheBucket.size()); ++cacheIndex)
		{
			if (cacheBucket[cacheIndex].getPlatformPipelineStateDesc() == platformPipelineStateDesc)
			{
				return &cacheBucket[cacheIndex];
			}
		}

		return nullptr;
	}

	PlatformPipelineStateObjectType* find(const PlatformPipelineStateDescType& platformPipelineStateDesc)
	{
		return find(platformPipelineStateDesc.getHashValue(), platformPipelineStateDesc);
	}

	PlatformPipelineStateObjectType* addOrGet(
		const uint64 hashValue,
		const PlatformPipelineStateDescType& platformPipelineStateDesc,
		PlatformPipelineStateObjectType&& pipelineStateObject)
	{
		CacheBucket& cacheBucket = pipelineStateObjectCacheByHash[hashValue];
		for (uint32 cacheIndex = 0; cacheIndex < static_cast<uint32>(cacheBucket.size()); ++cacheIndex)
		{
			if (cacheBucket[cacheIndex].getPlatformPipelineStateDesc() == platformPipelineStateDesc)
			{
				return &cacheBucket[cacheIndex];
			}
		}

		cacheBucket.push_back(moveValue(pipelineStateObject));
		const uint32 lastCacheIndex = static_cast<uint32>(cacheBucket.size()) - 1u;
		return &cacheBucket[lastCacheIndex];
	}

	PlatformPipelineStateObjectType* addOrGet(
		const PlatformPipelineStateDescType& platformPipelineStateDesc,
		PlatformPipelineStateObjectType&& pipelineStateObject)
	{
		return addOrGet(
			platformPipelineStateDesc.getHashValue(),
			platformPipelineStateDesc,
			moveValue(pipelineStateObject));
	}

	void clear()
	{
		pipelineStateObjectCacheByHash.clear();
	}

	uint32 getCachedCount() const
	{
		uint32 cacheCount = 0;
		for (const auto& cacheBucketPair : pipelineStateObjectCacheByHash)
		{
			cacheCount += static_cast<uint32>(cacheBucketPair.second.size());
		}
		return cacheCount;
	}

private:
	static constexpr uint32 cacheBucketInlineCapacity = 4;

	using CacheBucket = InplaceVector<PlatformPipelineStateObjectType, cacheBucketInlineCapacity>;
	unordered_map<uint64, CacheBucket> pipelineStateObjectCacheByHash;
};
