#pragma once

#include "Engine/Module/Asset/MeshParser.h"
#include "Engine/Module/Module.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Render/ResourceObject.h"
#include "Render/ResourceTypes.h"

class RenderBackend;

enum class MeshAssetHandleState : uint32
{
	pending = 0,
	ready = 1,
	failed = 2,
};

enum class MeshAssetGpuState : uint32
{
	none = 0,
	pending = 1,
	ready = 2,
	failed = 3,
};

struct MeshAssetHandle
{
	string meshRelativePath = {};
	uint32 lodLevel = 0;
	MeshAssetHandleState state = MeshAssetHandleState::pending;
	MeshAssetGpuState gpuState = MeshAssetGpuState::none;
	shared_pointer<MeshAsset> meshAsset = nullptr;
	unique_pointer<BufferResourceObject> vertexBufferObject = nullptr;
	unique_pointer<BufferResourceObject> indexBufferObject = nullptr;
	uint32 vertexBufferSizeInBytes = 0;
	uint32 indexBufferSizeInBytes = 0;
	uint32 vertexStrideInBytes = static_cast<uint32>(sizeof(MeshAsset::VertexData));
};

class MeshStreaming final : public StaticModule<MeshStreaming>
{
public:
	MeshStreaming()
		: StaticModule("MeshStreaming")
	{
	}

	bool init(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	shared_pointer<MeshAssetHandle> requestMesh(
		const string& meshRelativePath,
		uint32 lodLevel = 0);
	void flushGpuRequests(RenderBackend& renderBackend);
	void clear();
	uint32 getPendingRequestCount() const;

private:
	void flushCpuRequests();
	bool uploadMeshHandleToGpu(
		RenderBackend& renderBackend,
		MeshAssetHandle& handle,
		string& outErrorText) const;
	string getMeshCacheKey(const string& meshRelativePath, uint32 lodLevel) const;
	bool resolveMeshAbsolutePath(const string& meshRelativePath, string& outAbsolutePath) const;

	unordered_map<string, shared_pointer<MeshAssetHandle>> handleCache;
	vector<shared_pointer<MeshAssetHandle>> pendingHandles;
	vector<shared_pointer<MeshAssetHandle>> pendingGpuUploadHandles;
};
