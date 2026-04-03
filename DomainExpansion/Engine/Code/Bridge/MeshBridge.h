#pragma once

#include "Bridge/BridgeHandle.h"
#include "Bridge/DefaultBridge.h"

struct MeshAssetHandle;

struct MeshBridgeObject
{
	struct StaticProperty
	{
		BridgeHandle entityHandle = invalidBridgeHandle;
		string meshAssetPath = {};
		uint32 lodLevel = 0;
		shared_pointer<MeshAssetHandle> meshAssetHandle = nullptr;
		vector<BridgeHandle> materialHandles = {};
	};

	struct DynamicProperty
	{
		bool visible = true;
	};

	struct ObjectDesc
	{
		StaticProperty staticProperty = {};
		DynamicProperty dynamicProperty = {};
	};
};

class MeshBridge final : public DefaultBridge<MeshBridgeObject>
{
public:
	MeshBridge(const MeshBridge&) = delete;
	MeshBridge& operator=(const MeshBridge&) = delete;
	MeshBridge(MeshBridge&&) = delete;
	MeshBridge& operator=(MeshBridge&&) = delete;

	static MeshBridge& get()
	{
		static MeshBridge meshBridge = {};
		return meshBridge;
	}

	HandleReference createMeshHandle(const ObjectDesc& objectDesc);
	bool isHandleAlive(PackedHandle packedHandle) const;
	const StaticData* getStaticData(PackedHandle packedHandle) const;
	const DynamicData* getDynamicData(PackedHandle packedHandle) const;
	void updateDynamicData(PackedHandle packedHandle, const DynamicData& dynamicData);
	void processFrame();

private:
	MeshBridge() = default;
	~MeshBridge() = default;
};
