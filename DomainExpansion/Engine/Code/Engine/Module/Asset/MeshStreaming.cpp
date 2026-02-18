#include "Engine/Module/Asset/MeshStreaming.h"

#include "Engine/Framework/FrameworkFileSystem.h"

bool MeshStreaming::init(Framework& framework)
{
	unused(framework);
	clear();
	return true;
}

void MeshStreaming::update()
{
}

void MeshStreaming::shutdown()
{
	clear();
}

shared_pointer<MeshAssetHandle> MeshStreaming::requestMesh(
	const string& meshRelativePath,
	const uint32 lodLevel)
{
	const string cacheKey = getMeshCacheKey(meshRelativePath, lodLevel);
	auto foundHandle = handleCache.find(cacheKey);
	if (foundHandle != handleCache.end())
	{
		return foundHandle->second;
	}

	shared_pointer<MeshAssetHandle> handle(new MeshAssetHandle());
	handle->meshRelativePath = meshRelativePath;
	handle->lodLevel = lodLevel;
	handle->state = MeshAssetHandleState::pending;
	handleCache.emplace(cacheKey, handle);
	pendingHandles.push_back(handle);
	return handle;
}

void MeshStreaming::flushRequests()
{
	if (pendingHandles.empty())
	{
		return;
	}

	vector<shared_pointer<MeshAssetHandle>> processingHandles;
	processingHandles.swap(pendingHandles);

	for (uint32 handleIndex = 0; handleIndex < static_cast<uint32>(processingHandles.size()); ++handleIndex)
	{
		shared_pointer<MeshAssetHandle>& handle = processingHandles[handleIndex];
		if (handle == nullptr || handle->state != MeshAssetHandleState::pending)
		{
			continue;
		}

		string resolvedMeshPath = {};
		if (!resolveMeshAbsolutePath(handle->meshRelativePath, resolvedMeshPath))
		{
			handle->state = MeshAssetHandleState::failed;
			handle->errorText = "mesh_path_resolve_failed";
			error << "[MeshStreaming][Error] mesh=" << handle->meshRelativePath
				  << " lod=" << handle->lodLevel
				  << " reason=" << handle->errorText << lineBreak;
			continue;
		}

		MeshAsset meshAsset = {};
		string errorText = {};
		if (!MeshParser::get().parseFromFile(resolvedMeshPath, handle->lodLevel, meshAsset, errorText))
		{
			handle->state = MeshAssetHandleState::failed;
			handle->errorText = errorText.empty() ? "mesh_load_failed" : errorText;
			error << "[MeshStreaming][Error] mesh=" << handle->meshRelativePath
				  << " lod=" << handle->lodLevel
				  << " reason=" << handle->errorText << lineBreak;
			continue;
		}

		handle->meshAsset = shared_pointer<MeshAsset>(new MeshAsset(moveValue(meshAsset)));
		handle->state = MeshAssetHandleState::ready;
		handle->errorText.clear();
		output << "[MeshStreaming][Ready] mesh=" << handle->meshRelativePath
			   << " lod=" << handle->lodLevel
			   << " vertexCount=" << handle->meshAsset->vertexCount
			   << " indexCount=" << handle->meshAsset->indexCount << lineBreak;
	}
}

void MeshStreaming::clear()
{
	handleCache.clear();
	pendingHandles.clear();
}

uint32 MeshStreaming::getPendingRequestCount() const
{
	return static_cast<uint32>(pendingHandles.size());
}

string MeshStreaming::getMeshCacheKey(
	const string& meshRelativePath,
	const uint32 lodLevel) const
{
	return meshRelativePath + "|LOD" + std::to_string(lodLevel);
}

bool MeshStreaming::resolveMeshAbsolutePath(
	const string& meshRelativePath,
	string& outAbsolutePath) const
{
	outAbsolutePath.clear();
	return frameworkFileSystemResolvePathFromResources(meshRelativePath, outAbsolutePath);
}
