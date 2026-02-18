#pragma once

#include "Engine/Module/Asset/MeshParser.h"
#include "Engine/Module/Module.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Render/ResourceTypes.h"

enum class MeshAssetHandleState : uint32
{
	pending = 0,
	ready = 1,
	failed = 2,
};

struct MeshAssetHandle
{
	string meshRelativePath = {};
	uint32 lodLevel = 0;
	MeshAssetHandleState state = MeshAssetHandleState::pending;
	shared_pointer<MeshAsset> meshAsset = nullptr;
	string errorText = {};
};

class MeshStreaming final : public StaticModule<MeshStreaming>
{
public:
	MeshStreaming()
		: StaticModule("MeshStreaming")
	{
	}

	bool init(Framework& framework) override final;
	void update() override final;
	void shutdown() override final;

	shared_pointer<MeshAssetHandle> requestMesh(
		const string& meshRelativePath,
		uint32 lodLevel = 0);
	void flushRequests();
	void clear();
	uint32 getPendingRequestCount() const;

private:
	string getMeshCacheKey(const string& meshRelativePath, uint32 lodLevel) const;
	bool resolveMeshAbsolutePath(const string& meshRelativePath, string& outAbsolutePath) const;

	unordered_map<string, shared_pointer<MeshAssetHandle>> handleCache;
	vector<shared_pointer<MeshAssetHandle>> pendingHandles;
};
