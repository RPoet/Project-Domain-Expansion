#pragma once

#include <d3d12.h>
#include "Render/CommandList.h"

class Dx12CommandList final : public CommandList
{
public:
	Dx12CommandList() = default;
	bool initialize(const CommandListInitializeOptions& initializeOptions) override;
	void shutdown() override;

	void reset() override;
	void resourceBarrier(
		ResourceObject* resourceObject,
		ResourceState beforeState,
		ResourceState afterState) override;
	void setRenderTarget(RenderTargetView* renderTargetView) override;
	void clearRenderTarget(
		RenderTargetView* renderTargetView,
		float red,
		float green,
		float blue,
		float alpha) override;
	void setViewport(const ViewportArea& viewportArea) override;
	void setScissorRect(const ScissorRectArea& scissorRectArea) override;
	void setPrimitiveTopology(PrimitiveTopology primitiveTopology) override;
	void setVertexBuffer(uint32 slotIndex, const VertexBufferBinding& vertexBufferBinding) override;
	void setIndexBuffer(const IndexBufferBinding& indexBufferBinding) override;
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

	com_pointer<ID3D12CommandAllocator> commandAllocator;
	com_pointer<ID3D12GraphicsCommandList> commandList;
	bool recordingAvailable = false;
};
