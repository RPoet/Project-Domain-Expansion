#pragma once

#include "Engine/Assets/MeshAsset.h"
#include "Engine/Module/Module.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Render/Backends/ResourceObject.h"
#include "Render/Backends/ResourceTypes.h"

class RenderBackend;
class CommandList;
class GPUUploader;

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

enum class MeshBufferSignature : uint32
{
	position = 0,
	normal = 1,
	texcoord = 2,
	count = 3,
};

inline constexpr uint32 meshVertexBufferSignatureCount = static_cast<uint32>(MeshBufferSignature::count);

inline constexpr uint32 getMeshBufferSignatureIndex(const MeshBufferSignature signature)
{
	const uint32 signatureIndex = static_cast<uint32>(signature);
	if (signatureIndex >= meshVertexBufferSignatureCount)
	{
		return uint32MaxValue;
	}

	return signatureIndex;
}

inline constexpr uint32 getMeshBufferSignatureFlag(const MeshBufferSignature signature)
{
	const uint32 signatureIndex = getMeshBufferSignatureIndex(signature);
	if (signatureIndex == uint32MaxValue)
	{
		return 0;
	}

	return static_cast<uint32>(1u << signatureIndex);
}

struct MeshAssetHandle
{
	string meshAssetPath = {};
	uint32 lodLevel = 0;
	MeshAssetHandleState state = MeshAssetHandleState::pending;
	MeshAssetGpuState gpuState = MeshAssetGpuState::none;
	shared_pointer<MeshAsset> meshAsset = nullptr;
	unique_pointer<BufferResourceObject> vertexBufferObjects[meshVertexBufferSignatureCount] = {};
	uint32 vertexBufferSizesInBytes[meshVertexBufferSignatureCount] = {};
	uint32 vertexBufferStridesInBytes[meshVertexBufferSignatureCount] = { static_cast<uint32>(sizeof(PositionData)), static_cast<uint32>(sizeof(NormalData)), static_cast<uint32>(sizeof(TexcoordData)) };
	unique_pointer<BufferResourceObject> indexBufferObject = nullptr;
	uint32 indexBufferSizeInBytes = 0;
	uint32 requiredVertexBufferFlags = 0;
	uint32 activeVertexBufferFlags = 0;

	BufferResourceObject* getBufferObject(const MeshBufferSignature signature)
	{
		const uint32 signatureIndex = getMeshBufferSignatureIndex(signature);
		if (signatureIndex == uint32MaxValue)
		{
			return nullptr;
		}

		return vertexBufferObjects[signatureIndex].get();
	}

	const BufferResourceObject* getBufferObject(const MeshBufferSignature signature) const
	{
		const uint32 signatureIndex = getMeshBufferSignatureIndex(signature);
		if (signatureIndex == uint32MaxValue)
		{
			return nullptr;
		}

		return vertexBufferObjects[signatureIndex].get();
	}

	uint32 getBufferSizeInBytes(const MeshBufferSignature signature) const
	{
		const uint32 signatureIndex = getMeshBufferSignatureIndex(signature);
		if (signatureIndex == uint32MaxValue)
		{
			return 0;
		}

		return vertexBufferSizesInBytes[signatureIndex];
	}

	uint32 getBufferStrideInBytes(const MeshBufferSignature signature) const
	{
		const uint32 signatureIndex = getMeshBufferSignatureIndex(signature);
		if (signatureIndex == uint32MaxValue)
		{
			return 0;
		}

		return vertexBufferStridesInBytes[signatureIndex];
	}
};

// TO DO : Implement real mesh streamer
// current implementation is just a mesh uploader to the GPU after creating mesh lod, supplement missed part.
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
		const shared_pointer<MeshAsset>& meshAsset,
		uint32 lodLevel = 0);
	void flushGpuRequests(RenderBackend& renderBackend);
	void clear();
	uint32 getPendingRequestCount() const;

private:
	struct InFlightUploadSubmission
	{
		CommandList* commandList = nullptr;
		uint64 syncValue = 0;
	};

	void flushCpuRequests();
	bool uploadMeshHandleToGpu(
		RenderBackend& renderBackend,
		MeshAssetHandle& handle) const;
	void releaseCompletedUploadSubmissions(RenderBackend& renderBackend, GPUUploader& gpuUploader, uint64 completedUploadSyncValue);
	string getMeshCacheKey(const string& meshAssetPath, uint32 lodLevel) const;

	unordered_map<string, shared_pointer<MeshAssetHandle>> handleCache;
	vector<shared_pointer<MeshAssetHandle>> pendingGpuUploadHandles;
	vector<InFlightUploadSubmission> inFlightUploadSubmissions;
};
