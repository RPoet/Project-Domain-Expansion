#pragma once

#include <d3d12.h>
#include "Render/CommandQueue.h"

class Dx12CommandQueue final : public CommandQueue
{
public:
	Dx12CommandQueue() = default;

	void setNativeCommandQueue(com_pointer<ID3D12CommandQueue> commandQueue);
	ID3D12CommandQueue* getNativeCommandQueue() const;
	void enqueue(CommandList* commandListInterface) override;
	void executeQueued() override;
	void clearQueued() override;

private:
	com_pointer<ID3D12CommandQueue> commandQueue;
	vector<ID3D12CommandList*> queuedCommandLists;
};
