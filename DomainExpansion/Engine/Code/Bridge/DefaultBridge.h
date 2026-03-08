#pragma once

#include "Bridge/BaseBridge.h"
#include "Engine/Module/Bridge/BridgeModule.h"

template <typename object_type, uint32 default_size = 4096>
class DefaultBridge : public BaseBridge
{
public:
	using BridgeType = DefaultBridge<object_type, default_size>;
	using ObjectDesc = typename object_type::ObjectDesc;
	using StaticProperty = typename object_type::StaticProperty;
	using DynamicProperty = typename object_type::DynamicProperty;
	using StaticData = StaticProperty;
	using DynamicData = DynamicProperty;
	using PackedHandle = uint32;

	inline static constexpr uint32 handleIndexBits = 24;
	inline static constexpr uint32 handleGenerationBits = 8;
	inline static constexpr uint32 maxObjectCount = default_size;
	inline static constexpr PackedHandle invalidPackedHandle = uint32MaxValue;
	inline static constexpr uint32 handleIndexMask = (1u << handleIndexBits) - 1u;
	inline static constexpr uint32 handleGenerationMask = (1u << handleGenerationBits) - 1u;

	class HandleReference
	{
	public:
		HandleReference() = default;
		HandleReference(DefaultBridge* ownerBridge, const PackedHandle handle)
			: owner(ownerBridge)
			, packedHandle(handle)
		{
		}

		~HandleReference()
		{
			reset();
		}

		HandleReference(const HandleReference&) = delete;
		HandleReference& operator=(const HandleReference&) = delete;

		HandleReference(HandleReference&& other) noexcept
			: owner(other.owner)
			, packedHandle(other.packedHandle)
		{
			other.owner = nullptr;
			other.packedHandle = invalidPackedHandle;
		}

		HandleReference& operator=(HandleReference&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}

			reset();
			owner = other.owner;
			packedHandle = other.packedHandle;
			other.owner = nullptr;
			other.packedHandle = invalidPackedHandle;
			return *this;
		}

		void reset()
		{
			if (owner == nullptr || packedHandle == invalidPackedHandle)
			{
				return;
			}

			owner->requestDelete(packedHandle);
			owner = nullptr;
			packedHandle = invalidPackedHandle;
		}

		bool isValid() const
		{
			return owner != nullptr && packedHandle != invalidPackedHandle;
		}

		PackedHandle getPackedHandle() const
		{
			return packedHandle;
		}

		PackedHandle release()
		{
			const PackedHandle releasedHandle = packedHandle;
			owner = nullptr;
			packedHandle = invalidPackedHandle;
			return releasedHandle;
		}

		uint32 getIndex() const
		{
			return packedHandle & handleIndexMask;
		}

		uint32 getGeneration() const
		{
			return (packedHandle >> handleIndexBits) & handleGenerationMask;
		}

	private:
		DefaultBridge* owner = nullptr;
		PackedHandle packedHandle = invalidPackedHandle;
	};

private:
	struct PendingDynamicUpdate
	{
		PackedHandle packedHandle = invalidPackedHandle;
		DynamicProperty dynamicProperty = {};
	};

	struct PendingDelete
	{
		PackedHandle packedHandle = invalidPackedHandle;
		uint64 executeFrameSerial = 0;
	};

public:
	DefaultBridge()
	{
		static_assert(default_size > 0, "DefaultBridge default_size must be greater than zero.");
		static_assert(handleIndexBits + handleGenerationBits == 32, "DefaultBridge handle bits must sum to 32.");
		static_assert(default_size <= (1u << handleIndexBits), "DefaultBridge default_size exceeds handle index capacity.");
		BridgeModule::get()->registerBridge(this);
		for (uint32 slotIndex = 0; slotIndex < default_size; ++slotIndex)
		{
			slotGeneration[slotIndex] = 1;
		}
	}

	~DefaultBridge() override
	{
		BridgeModule::get()->unregisterBridge(this);
	}

	HandleReference createObject(const ObjectDesc& objectDesc)
	{
		uint32 slotIndex = uint32MaxValue;
		if (!freeIndices.empty())
		{
			slotIndex = freeIndices.back();
			freeIndices.pop_back();
		}
		else
		{
			assert(nextUnusedIndex < default_size);
			if (nextUnusedIndex >= default_size)
			{
				return HandleReference();
			}

			slotIndex = nextUnusedIndex;
			++nextUnusedIndex;
		}

		slotAlive[slotIndex] = true;
		staticProperties[slotIndex] = objectDesc.staticProperty;
		dynamicProperties[slotIndex] = objectDesc.dynamicProperty;
		const PackedHandle packedHandle = packHandle(slotIndex, slotGeneration[slotIndex]);
		return HandleReference(this, packedHandle);
	}

	void updateObject(const HandleReference& handleReference, const DynamicProperty& dynamicProperty)
	{
		updateObject(handleReference.getPackedHandle(), dynamicProperty);
	}

	void updateObject(const PackedHandle packedHandle, const DynamicProperty& dynamicProperty)
	{
		if (!isHandleAlive(packedHandle))
		{
			return;
		}

		PendingDynamicUpdate pendingUpdate = {};
		pendingUpdate.packedHandle = packedHandle;
		pendingUpdate.dynamicProperty = dynamicProperty;
		pendingDynamicUpdates.push_back(moveValue(pendingUpdate));
	}

	bool isHandleAlive(const HandleReference& handleReference) const
	{
		return isHandleAlive(handleReference.getPackedHandle());
	}

	bool isHandleAlive(const PackedHandle packedHandle) const
	{
		if (packedHandle == invalidPackedHandle)
		{
			return false;
		}

		const uint32 slotIndex = unpackHandleIndex(packedHandle);
		if (slotIndex >= nextUnusedIndex)
		{
			return false;
		}

		if (!slotAlive[slotIndex])
		{
			return false;
		}

		return slotGeneration[slotIndex] == unpackHandleGeneration(packedHandle);
	}

	const StaticProperty* getStaticProperty(const HandleReference& handleReference) const
	{
		return getStaticProperty(handleReference.getPackedHandle());
	}

	const StaticProperty* getStaticProperty(const PackedHandle packedHandle) const
	{
		if (!isHandleAlive(packedHandle))
		{
			return nullptr;
		}

		return &staticProperties[unpackHandleIndex(packedHandle)];
	}

	const DynamicProperty* getDynamicProperty(const HandleReference& handleReference) const
	{
		return getDynamicProperty(handleReference.getPackedHandle());
	}

	const DynamicProperty* getDynamicProperty(const PackedHandle packedHandle) const
	{
		if (!isHandleAlive(packedHandle))
		{
			return nullptr;
		}

		return &dynamicProperties[unpackHandleIndex(packedHandle)];
	}

	PackedHandle getPackedHandleBySlotIndex(const uint32 slotIndex) const
	{
		if (slotIndex >= nextUnusedIndex || !slotAlive[slotIndex])
		{
			return invalidPackedHandle;
		}

		return packHandle(slotIndex, slotGeneration[slotIndex]);
	}

	void requestDelete(const PackedHandle packedHandle)
	{
		if (!isHandleAlive(packedHandle))
		{
			return;
		}

		for (uint32 deleteIndex = 0; deleteIndex < static_cast<uint32>(pendingDeletes.size()); ++deleteIndex)
		{
			if (pendingDeletes[deleteIndex].packedHandle == packedHandle)
			{
				return;
			}
		}

		PendingDelete pendingDelete = {};
		pendingDelete.packedHandle = packedHandle;
		pendingDelete.executeFrameSerial = frameSerial;
		pendingDeletes.push_back(pendingDelete);
	}

	void processFrame() override
	{
		++frameSerial;

		for (uint32 updateIndex = 0; updateIndex < static_cast<uint32>(pendingDynamicUpdates.size()); ++updateIndex)
		{
			const PendingDynamicUpdate& pendingUpdate = pendingDynamicUpdates[updateIndex];
			if (!isHandleAlive(pendingUpdate.packedHandle))
			{
				continue;
			}

			dynamicProperties[unpackHandleIndex(pendingUpdate.packedHandle)] = pendingUpdate.dynamicProperty;
		}
		pendingDynamicUpdates.clear();

		vector<PendingDelete> remainingDeletes = {};
		remainingDeletes.reserve(pendingDeletes.size());
		for (uint32 deleteIndex = 0; deleteIndex < static_cast<uint32>(pendingDeletes.size()); ++deleteIndex)
		{
			const PendingDelete& pendingDelete = pendingDeletes[deleteIndex];
			if (pendingDelete.executeFrameSerial > frameSerial)
			{
				remainingDeletes.push_back(pendingDelete);
				continue;
			}

			if (!isHandleAlive(pendingDelete.packedHandle))
			{
				continue;
			}

			const uint32 slotIndex = unpackHandleIndex(pendingDelete.packedHandle);
			slotAlive[slotIndex] = false;
			staticProperties[slotIndex] = {};
			dynamicProperties[slotIndex] = {};
			slotGeneration[slotIndex] = incrementGeneration(slotGeneration[slotIndex]);
			freeIndices.push_back(slotIndex);
		}
		pendingDeletes.swap(remainingDeletes);
	}

private:
	inline static PackedHandle packHandle(const uint32 slotIndex, const uint32 generation)
	{
		return (slotIndex & handleIndexMask) | ((generation & handleGenerationMask) << handleIndexBits);
	}

	inline static uint32 unpackHandleIndex(const PackedHandle packedHandle)
	{
		return packedHandle & handleIndexMask;
	}

	inline static uint32 unpackHandleGeneration(const PackedHandle packedHandle)
	{
		return (packedHandle >> handleIndexBits) & handleGenerationMask;
	}

	inline static uint32 incrementGeneration(const uint32 generation)
	{
		uint32 nextGeneration = (generation + 1u) & handleGenerationMask;
		if (nextGeneration == 0)
		{
			nextGeneration = 1;
		}
		return nextGeneration;
	}

	uint64 frameSerial = 0;
	uint32 nextUnusedIndex = 0;
	bool slotAlive[default_size] = {};
	uint32 slotGeneration[default_size] = {};
	StaticProperty staticProperties[default_size] = {};
	DynamicProperty dynamicProperties[default_size] = {};
	vector<uint32> freeIndices = {};
	vector<PendingDynamicUpdate> pendingDynamicUpdates = {};
	vector<PendingDelete> pendingDeletes = {};
};
