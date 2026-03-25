#pragma once

#include "Engine/Module/Module.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Render/ResourceObject.h"
#include "Render/SyncObject.h"

class CommandList;
class RenderBackend;

enum class GPUUploaderMode : uint32
{
	staging = 0,
	pool = 1,
};

struct BufferUploadRequestOptions
{
	const void* sourceData = nullptr;
	uint64 sourceDataSizeInBytes = 0;
	uint64 destinationOffsetInBytes = 0;
};

class GPUUploader final : public StaticModule<GPUUploader>
{
public:
	GPUUploader()
		: StaticModule("GPUUploader")
	{
	}

	bool init(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	unique_pointer<BufferResourceObject> createBufferObject(
		RenderBackend& renderBackend,
		const BufferObjectCreateOptions& createOptions,
		const BufferUploadRequestOptions& uploadRequestOptions);
	void refreshCompletedSyncValue();
	void signalUploadSync();
	void uploadQueuedBuffers(CommandList& commandList);
	bool hasQueuedUploadRequests() const;

private:
	struct UploadBufferPoolBlock
	{
		unique_pointer<BufferResourceObject> bufferObject = nullptr;
		char* mappedMemory = nullptr;
		uint64 capacityInBytes = 0;
		uint64 usedInBytes = 0;
		uint64 lastUsedSyncValue = 0;
		bool pendingSubmission = false;
	};

	struct QueuedUploadRequest
	{
		BufferResourceObject* destinationBufferObject = nullptr;
		BufferResourceObject* sourceBufferObject = nullptr;
		uint64 destinationOffsetInBytes = 0;
		uint64 sourceOffsetInBytes = 0;
		uint64 copySizeInBytes = 0;
	};

	static constexpr uint64 minimumPoolBlockSizeInBytes = 64ull * 1024ull;
	static constexpr uint64 idleReleaseSyncValueThreshold = 180;

	bool reserveUploadSpace(
		RenderBackend& renderBackend,
		uint64 requestSizeInBytes,
		uint32& outBlockIndex,
		uint64& outBlockOffsetInBytes);
	bool createUploadPoolBlock(RenderBackend& renderBackend, uint64 requestSizeInBytes);
	static void clearUploadBufferBlock(UploadBufferPoolBlock& poolBlock);
	void releaseIdlePoolBlocks();
	void resetFrameAllocations();
	void clearPool();
	void initializeUploadSyncObject(RenderBackend& renderBackend);

	GPUUploaderMode uploaderMode = GPUUploaderMode::staging;
	uint64 completedUploadSyncValue = 0;
	uint64 lastSubmittedUploadSyncValue = 0;
	unique_pointer<SyncObject> uploadSyncObject = nullptr;
	vector<UploadBufferPoolBlock> uploadBufferPoolBlocks;
	vector<QueuedUploadRequest> queuedUploadRequests;
};
