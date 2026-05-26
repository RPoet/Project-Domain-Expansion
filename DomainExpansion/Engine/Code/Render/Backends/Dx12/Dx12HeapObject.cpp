#include "Render/Backends/Dx12/Dx12HeapObject.h"

Dx12HeapObject::Dx12HeapObject(com_pointer<ID3D12Heap> dx12Heap)
	: heap(moveValue(dx12Heap))
{
}

const com_pointer<ID3D12Heap>& Dx12HeapObject::getUnderlyingHeap() const
{
	return heap;
}

const void* Dx12HeapObject::getNativeHeap() const
{
	return heap.Get();
}

void* Dx12HeapObject::getNativeHeap()
{
	return heap.Get();
}
