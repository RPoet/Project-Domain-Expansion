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

void Dx12CommandQueue::execute(CommandList* commandListInterface)
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

	ID3D12GraphicsCommandList* nativeCommandList = dx12CommandList->getNativeCommandList();
	if (nativeCommandList == nullptr)
	{
		return;
	}

	ID3D12CommandList* commandLists[] = { nativeCommandList };
	commandQueue->ExecuteCommandLists(1, commandLists);
}
