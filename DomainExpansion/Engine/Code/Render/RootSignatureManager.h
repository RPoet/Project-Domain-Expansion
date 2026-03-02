#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/RootSignatureObject.h"

template <typename PlatformRootSignatureDescType, typename PlatformRootSignatureObjectType>
class RootSignatureManager
{
public:
	PlatformRootSignatureObjectType* find(
		const uint64 hashValue,
		const PlatformRootSignatureDescType& platformRootSignatureDesc)
	{
		auto foundBucket = rootSignatureObjectCacheByHash.find(hashValue);
		if (foundBucket == rootSignatureObjectCacheByHash.end())
		{
			return nullptr;
		}

		CacheBucket& cacheBucket = foundBucket->second;
		for (uint32 cacheIndex = 0; cacheIndex < static_cast<uint32>(cacheBucket.size()); ++cacheIndex)
		{
			if (cacheBucket[cacheIndex].getPlatformRootSignatureDesc() == platformRootSignatureDesc)
			{
				return &cacheBucket[cacheIndex];
			}
		}

		return nullptr;
	}

	PlatformRootSignatureObjectType* find(const PlatformRootSignatureDescType& platformRootSignatureDesc)
	{
		return find(platformRootSignatureDesc.getHashValue(), platformRootSignatureDesc);
	}

	PlatformRootSignatureObjectType* addOrGet(
		const uint64 hashValue,
		const PlatformRootSignatureDescType& platformRootSignatureDesc,
		PlatformRootSignatureObjectType&& rootSignatureObject)
	{
		CacheBucket& cacheBucket = rootSignatureObjectCacheByHash[hashValue];
		for (uint32 cacheIndex = 0; cacheIndex < static_cast<uint32>(cacheBucket.size()); ++cacheIndex)
		{
			if (cacheBucket[cacheIndex].getPlatformRootSignatureDesc() == platformRootSignatureDesc)
			{
				return &cacheBucket[cacheIndex];
			}
		}

		cacheBucket.push_back(moveValue(rootSignatureObject));
		const uint32 lastCacheIndex = static_cast<uint32>(cacheBucket.size()) - 1u;
		return &cacheBucket[lastCacheIndex];
	}

	PlatformRootSignatureObjectType* addOrGet(
		const PlatformRootSignatureDescType& platformRootSignatureDesc,
		PlatformRootSignatureObjectType&& rootSignatureObject)
	{
		return addOrGet(
			platformRootSignatureDesc.getHashValue(),
			platformRootSignatureDesc,
			moveValue(rootSignatureObject));
	}

	void clear()
	{
		rootSignatureObjectCacheByHash.clear();
	}

	uint32 getCachedCount() const
	{
		uint32 cacheCount = 0;
		for (const auto& cacheBucketPair : rootSignatureObjectCacheByHash)
		{
			cacheCount += static_cast<uint32>(cacheBucketPair.second.size());
		}
		return cacheCount;
	}

private:
	static constexpr uint32 cacheBucketInlineCapacity = 4;

	using CacheBucket = InplaceVector<PlatformRootSignatureObjectType, cacheBucketInlineCapacity>;
	unordered_map<uint64, CacheBucket> rootSignatureObjectCacheByHash;
};
