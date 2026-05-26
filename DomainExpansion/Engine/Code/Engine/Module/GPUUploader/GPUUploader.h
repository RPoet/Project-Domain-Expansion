#pragma once

#include "Engine/Module/Module.h"
#include "Render/GPUBufferAllocator.h"
#include "Render/Backends/ResourceObject.h"

class CommandList;
class RenderBackend;
class SyncObject;

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
	GPUUploader();
	~GPUUploader() override final;

	bool initialize(Framework& framework) override final;
	void preUpdate() override final;
	void postUpdate() override final;
	void shutdown() override final;

	unique_pointer<BufferResourceObject> createBufferObject(RenderBackend& renderBackend, const BufferObjectCreateOptions& createOptions, const BufferUploadRequestOptions& uploadRequestOptions);
	void refreshCompletedSyncValue();
	uint64 signalUploadSync();
	uint64 getCompletedUploadSyncValue() const;
	void waitForUploadCompletion();
	void recycleCompletedUploadBuffers();
	void uploadQueuedBuffers(CommandList& commandList);
	bool hasQueuedUploadRequests() const;
	bool isQueuedUploadRequestThresholdReached() const;
	uint64 getQueuedUploadRequestSizeInBytes() const;

private:
	struct UploadHeapBufferAllocation
	{
		BufferResourceObject* bufferObject = nullptr;
		char* mappedMemory = nullptr;
		uint64 offsetInBytes = 0;
		uint64 sizeInBytes = 0;
	};

	class UploadHeapBuffer
	{
	public:
		void initialize(unique_pointer<BufferResourceObject>&& inBufferObject, char* inMappedMemory, uint64 inCapacityInBytes);
		bool isValid() const;
		bool canAllocate(uint64 requestSizeInBytes) const;
		UploadHeapBufferAllocation allocate(uint64 requestSizeInBytes);
		void resetAllocations();
		void clear();

	private:
		unique_pointer<BufferResourceObject> bufferObject = nullptr;
		char* mappedMemory = nullptr;
		uint64 capacityInBytes = 0;
		uint64 usedInBytes = 0;
	};

	struct UploadBufferPoolBlock
	{
		UploadHeapBuffer uploadBuffer;
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

	static constexpr uint64 defaultPoolBlockSizeInBytes = 32ull * 1024ull * 1024ull;
	static constexpr uint64 queuedUploadSubmitThresholdInBytes = defaultPoolBlockSizeInBytes;
	static constexpr uint64 idleReleaseSyncValueThreshold = 180;

	void reserveUploadSpace(RenderBackend& renderBackend, uint64 requestSizeInBytes, UploadHeapBufferAllocation& outAllocation);
	static void clearUploadBufferBlock(UploadBufferPoolBlock& poolBlock);
	void releaseIdlePoolBlocks();
	void clearPool();
	void initializeUploadSyncObject(RenderBackend& renderBackend);

	GPUUploaderMode uploaderMode = GPUUploaderMode::staging;
	GPUBufferAllocator bufferAllocator;
	uint64 completedUploadSyncValue = 0;
	uint64 lastSubmittedUploadSyncValue = 0;
	unique_pointer<SyncObject> uploadSyncObject = nullptr;
	vector<UploadBufferPoolBlock> uploadBufferPoolBlocks;
	vector<QueuedUploadRequest> queuedUploadRequests;
	uint64 queuedUploadRequestSizeInBytes = 0;
};
