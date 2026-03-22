#pragma once

#include "Engine/Assets/MeshAsset.h"
#include "Engine/Module/MeshParser/MeshParser.h"
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
	string meshRelativePath = {};
	uint32 lodLevel = 0;
	MeshAssetHandleState state = MeshAssetHandleState::pending;
	MeshAssetGpuState gpuState = MeshAssetGpuState::none;
	shared_pointer<MeshAsset> meshAsset = nullptr;
	unique_pointer<BufferResourceObject> vertexBufferObjects[meshVertexBufferSignatureCount] = {};
	uint32 vertexBufferSizesInBytes[meshVertexBufferSignatureCount] = {};
	uint32 vertexBufferStridesInBytes[meshVertexBufferSignatureCount] =
	{
		static_cast<uint32>(sizeof(MeshAsset::PositionData)),
		static_cast<uint32>(sizeof(MeshAsset::NormalData)),
		static_cast<uint32>(sizeof(MeshAsset::TexcoordData))
	};
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
		MeshAssetHandle& handle) const;
	string getMeshCacheKey(const string& meshRelativePath, uint32 lodLevel) const;
	bool resolveMeshAbsolutePath(const string& meshRelativePath, string& outAbsolutePath) const;

	unordered_map<string, shared_pointer<MeshAssetHandle>> handleCache;
	vector<shared_pointer<MeshAssetHandle>> pendingHandles;
	vector<shared_pointer<MeshAssetHandle>> pendingGpuUploadHandles;
};
