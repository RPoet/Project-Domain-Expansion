#pragma once

#include <d3d12.h>
#include "Render/CommandList.h"

class Dx12CommandList final : public CommandList
{
public:
	Dx12CommandList() = default;
	bool initialize(const CommandListInitializeOptions& initializeOptions) override;
	void shutdown() override;

	void assignCommandAllocator(ID3D12CommandAllocator* commandAllocator);
	ID3D12CommandAllocator* detachCommandAllocator();
	void assignCommandList(ID3D12GraphicsCommandList* commandList);
	ID3D12GraphicsCommandList* detachCommandList();

	void reset() override;
	void resourceBarrier(
		ResourceObject* resourceObject,
		ResourceState beforeState,
		ResourceState afterState) override;
	void setRenderTargets(
		RenderTargetView* const* renderTargetViews,
		uint32 renderTargetViewCount,
		DepthStencilView* depthStencilView) override;
	void clearRenderTarget(
		RenderTargetView* renderTargetView,
		float red,
		float green,
		float blue,
		float alpha) override;
	void clearDepthStencil(
		DepthStencilView* depthStencilView,
		float depthValue,
		uint32 stencilValue) override;
	void setViewport(const ViewportArea& viewportArea) override;
	void setScissorRect(const ScissorRectArea& scissorRectArea) override;
	void setPrimitiveTopology(PrimitiveTopology primitiveTopology) override;
	void setVertexBuffer(uint32 slotIndex, const VertexBufferBinding& vertexBufferBinding) override;
	void setIndexBuffer(const IndexBufferBinding& indexBufferBinding) override;
	void setPipeline(PipelineStateObject* pipelineStateObject, RootSignatureObject* rootSignatureObject) override;
	void setGraphicsPushConstants(
		uint32 pushConstantRangeIndex,
		const void* data,
		uint32 sizeInBytes) override;
	void copyBuffer(
		BufferResourceObject* destinationBufferObject,
		uint64 destinationOffsetInBytes,
		BufferResourceObject* sourceBufferObject,
		uint64 sourceOffsetInBytes,
		uint64 copySizeInBytes) override;
	void drawIndexed(
		uint32 indexCountPerInstance,
		uint32 instanceCount,
		uint32 startIndexLocation,
		int32 baseVertexLocation,
		uint32 startInstanceLocation) override;
	void draw(
		uint32 vertexCountPerInstance,
		uint32 instanceCount,
		uint32 startVertexLocation,
		uint32 startInstanceLocation) override;
	void close() override;
	ID3D12GraphicsCommandList* getNativeCommandList() const;

private:
	bool isRecordingReady() const;

	ID3D12CommandAllocator* commandAllocator = nullptr;
	ID3D12GraphicsCommandList* commandList = nullptr;
	bool recordingAvailable = false;
};
