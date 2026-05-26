#pragma once

#include <d3d12.h>
#include "Render/Backends/Dx12/Dx12RootSignatureDesc.h"
#include "Render/Backends/RootSignatureObject.h"

class Dx12RootSignatureObject final : public RootSignatureObject
{
private:
	Dx12RootSignatureDesc platformRootSignatureDesc = {};
	com_pointer<ID3D12RootSignature> rootSignature = {};

public:
	Dx12RootSignatureObject() = default;
	Dx12RootSignatureObject(
		const Dx12RootSignatureDesc& rootSignatureDesc,
		const com_pointer<ID3D12RootSignature>& nativeRootSignature)
		: platformRootSignatureDesc(rootSignatureDesc)
		, rootSignature(nativeRootSignature)
	{
	}

	const Dx12RootSignatureDesc& getPlatformRootSignatureDesc() const
	{
		return platformRootSignatureDesc;
	}

	com_pointer<ID3D12RootSignature>& getRootSignature()
	{
		return rootSignature;
	}

	const com_pointer<ID3D12RootSignature>& getRootSignature() const
	{
		return rootSignature;
	}
};
