#include "Render/Backends/Dx12/Dx12CommandQueue.h"
#include "Render/Backends/Dx12/Dx12CommandList.h"

void Dx12CommandQueue::setNativeCommandQueue(com_pointer<ID3D12CommandQueue> commandQueue)
{
	this->commandQueue = commandQueue;
}

ID3D12CommandQueue* Dx12CommandQueue::getNativeCommandQueue() const
{
	return commandQueue.Get();
}

void Dx12CommandQueue::enqueue(CommandList* commandListInterface)
{
	if (commandQueue == nullptr || commandListInterface == nullptr)
	{
		return;
	}

	Dx12CommandList* dx12CommandList = dynamic_cast<Dx12CommandList*>(commandListInterface);
	if (dx12CommandList == nullptr)
	{
		return;
	}

	ID3D12GraphicsCommandList* nativeGraphicsCommandList = dx12CommandList->getNativeCommandList();
	if (nativeGraphicsCommandList == nullptr)
	{
		return;
	}

	queuedCommandLists.push_back(nativeGraphicsCommandList);
}

void Dx12CommandQueue::executeQueued()
{
	if (commandQueue == nullptr || queuedCommandLists.empty())
	{
		return;
	}

	commandQueue->ExecuteCommandLists(static_cast<uint32>(queuedCommandLists.size()), queuedCommandLists.data());
}

void Dx12CommandQueue::clearQueued()
{
	queuedCommandLists.clear();
}
