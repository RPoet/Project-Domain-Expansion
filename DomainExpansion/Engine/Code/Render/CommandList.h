#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/RenderTargetView.h"
#include "Render/ResourceObject.h"
#include "Render/ResourceState.h"

class RenderBackend;

enum class CommandListType : uint32
{
	graphics = 0,
	compute = 1,
	copy = 2,
};

enum class PrimitiveTopology : uint32
{
	pointList = 0,
	lineList = 1,
	triangleList = 2,
};

enum class IndexElementSize : uint32
{
	sixteenBits = 0,
	thirtyTwoBits = 1,
};

struct ViewportArea
{
	float topLeftX = 0.0f;
	float topLeftY = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	float minDepth = 0.0f;
	float maxDepth = 1.0f;
};

struct ScissorRectArea
{
	int32 left = 0;
	int32 top = 0;
	int32 right = 0;
	int32 bottom = 0;
};

struct VertexBufferBinding
{
	BufferResourceObject* resourceObject = nullptr;
	uint32 strideInBytes = 0;
	uint32 sizeInBytes = 0;
	uint32 offsetInBytes = 0;
};

struct IndexBufferBinding
{
	BufferResourceObject* resourceObject = nullptr;
	IndexElementSize elementSize = IndexElementSize::thirtyTwoBits;
	uint32 sizeInBytes = 0;
	uint32 offsetInBytes = 0;
};

struct MeshDrawBindingState
{
	PrimitiveTopology primitiveTopology = PrimitiveTopology::triangleList;
	uint32 vertexBufferSlot = 0;
	VertexBufferBinding vertexBufferBinding = {};
	IndexBufferBinding indexBufferBinding = {};
};

inline bool isValidMeshDrawBindingState(const MeshDrawBindingState& meshDrawBindingState)
{
	return meshDrawBindingState.vertexBufferBinding.resourceObject != nullptr
		&& meshDrawBindingState.indexBufferBinding.resourceObject != nullptr
		&& meshDrawBindingState.vertexBufferBinding.strideInBytes > 0
		&& meshDrawBindingState.vertexBufferBinding.sizeInBytes > 0
		&& meshDrawBindingState.indexBufferBinding.sizeInBytes > 0;
}

struct CommandListInitializeOptions
{
	RenderBackend* renderBackend = nullptr;
	CommandListType commandListType = CommandListType::graphics;
};

class CommandList
{
public:
	virtual ~CommandList() = default;

	virtual bool initialize(const CommandListInitializeOptions& initializeOptions) = 0;
	virtual void shutdown() = 0;
	virtual void reset() = 0;
	virtual void resourceBarrier(
		ResourceObject* resourceObject,
		ResourceState beforeState,
		ResourceState afterState) = 0;
	// TO DO : Migrate render-target view binding to unified descriptor/view binding path.
	virtual void setRenderTarget(RenderTargetView* renderTargetView) = 0;
	// TO DO : Migrate clear operation to the same unified descriptor/view binding path.
	virtual void clearRenderTarget(
		RenderTargetView* renderTargetView,
		float red,
		float green,
		float blue,
		float alpha) = 0;
	virtual void setViewport(const ViewportArea& viewportArea) = 0;
	virtual void setScissorRect(const ScissorRectArea& scissorRectArea) = 0;
	virtual void setPrimitiveTopology(PrimitiveTopology primitiveTopology) = 0;
	virtual void setVertexBuffer(uint32 slotIndex, const VertexBufferBinding& vertexBufferBinding) = 0;
	virtual void setIndexBuffer(const IndexBufferBinding& indexBufferBinding) = 0;
	virtual void copyBuffer(
		BufferResourceObject* destinationBufferObject,
		uint64 destinationOffsetInBytes,
		BufferResourceObject* sourceBufferObject,
		uint64 sourceOffsetInBytes,
		uint64 copySizeInBytes) = 0;
	virtual void drawIndexed(
		uint32 indexCountPerInstance,
		uint32 instanceCount,
		uint32 startIndexLocation,
		int32 baseVertexLocation,
		uint32 startInstanceLocation) = 0;
	virtual void draw(
		uint32 vertexCountPerInstance,
		uint32 instanceCount,
		uint32 startVertexLocation,
		uint32 startInstanceLocation) = 0;
	virtual void close() = 0;

	bool bindMeshDrawState(const MeshDrawBindingState& meshDrawBindingState)
	{
		if (!isValidMeshDrawBindingState(meshDrawBindingState))
		{
			return false;
		}

		setPrimitiveTopology(meshDrawBindingState.primitiveTopology);
		setVertexBuffer(
			meshDrawBindingState.vertexBufferSlot,
			meshDrawBindingState.vertexBufferBinding);
		setIndexBuffer(meshDrawBindingState.indexBufferBinding);
		return true;
	}
};
