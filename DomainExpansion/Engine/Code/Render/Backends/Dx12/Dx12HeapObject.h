#pragma once

#include <d3d12.h>

#include "Render/Backends/HeapObject.h"

class Dx12HeapObject final : public HeapObject
{
public:
	explicit Dx12HeapObject(com_pointer<ID3D12Heap> dx12Heap);

	const com_pointer<ID3D12Heap>& getUnderlyingHeap() const;

	const void* getNativeHeap() const override;
	void* getNativeHeap() override;

private:
	com_pointer<ID3D12Heap> heap;
};
