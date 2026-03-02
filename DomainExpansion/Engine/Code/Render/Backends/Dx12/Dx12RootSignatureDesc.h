#pragma once

#include <d3d12.h>

#include "Render/RootSignatureObject.h"

inline bool isEqualDx12RootParameter(
	const D3D12_ROOT_PARAMETER& left,
	const D3D12_ROOT_PARAMETER& right)
{
	if (left.ParameterType != right.ParameterType
		|| left.ShaderVisibility != right.ShaderVisibility)
	{
		return false;
	}

	switch (left.ParameterType)
	{
	case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
		return left.Constants.ShaderRegister == right.Constants.ShaderRegister
			&& left.Constants.RegisterSpace == right.Constants.RegisterSpace
			&& left.Constants.Num32BitValues == right.Constants.Num32BitValues;
	case D3D12_ROOT_PARAMETER_TYPE_CBV:
	case D3D12_ROOT_PARAMETER_TYPE_SRV:
	case D3D12_ROOT_PARAMETER_TYPE_UAV:
		return left.Descriptor.ShaderRegister == right.Descriptor.ShaderRegister
			&& left.Descriptor.RegisterSpace == right.Descriptor.RegisterSpace;
	case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
	default:
		return false;
	}
}

struct Dx12RootSignatureDesc
{
	uint32 flags = 0;
	InplaceVector<D3D12_ROOT_PARAMETER, 8> rootParameters = {};

	D3D12_ROOT_SIGNATURE_DESC getNativeDesc() const
	{
		D3D12_ROOT_SIGNATURE_DESC nativeDesc = {};
		nativeDesc.NumParameters = static_cast<uint32>(rootParameters.size());
		nativeDesc.pParameters = rootParameters.empty() ? nullptr : rootParameters.data();
		nativeDesc.NumStaticSamplers = 0;
		nativeDesc.pStaticSamplers = nullptr;
		nativeDesc.Flags = static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(flags);
		return nativeDesc;
	}

	bool operator==(const Dx12RootSignatureDesc& other) const
	{
		if (flags != other.flags
			|| rootParameters.size() != other.rootParameters.size())
		{
			return false;
		}

		for (uint32 parameterIndex = 0; parameterIndex < static_cast<uint32>(rootParameters.size()); ++parameterIndex)
		{
			if (!isEqualDx12RootParameter(rootParameters[parameterIndex], other.rootParameters[parameterIndex]))
			{
				return false;
			}
		}

		return true;
	}

	uint64 getHashValue() const
	{
		uint64 hashValue = platformHashOffsetBasis;
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(flags));
		hashValue = platformHashCombine(hashValue, static_cast<uint64>(rootParameters.size()));

		for (uint32 parameterIndex = 0; parameterIndex < static_cast<uint32>(rootParameters.size()); ++parameterIndex)
		{
			const D3D12_ROOT_PARAMETER& parameter = rootParameters[parameterIndex];
			hashValue = platformHashCombine(hashValue, static_cast<uint64>(parameter.ParameterType));
			hashValue = platformHashCombine(hashValue, static_cast<uint64>(parameter.ShaderVisibility));

			switch (parameter.ParameterType)
			{
			case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				hashValue = platformHashCombine(hashValue, static_cast<uint64>(parameter.Constants.ShaderRegister));
				hashValue = platformHashCombine(hashValue, static_cast<uint64>(parameter.Constants.RegisterSpace));
				hashValue = platformHashCombine(hashValue, static_cast<uint64>(parameter.Constants.Num32BitValues));
				break;
			case D3D12_ROOT_PARAMETER_TYPE_CBV:
			case D3D12_ROOT_PARAMETER_TYPE_SRV:
			case D3D12_ROOT_PARAMETER_TYPE_UAV:
				hashValue = platformHashCombine(hashValue, static_cast<uint64>(parameter.Descriptor.ShaderRegister));
				hashValue = platformHashCombine(hashValue, static_cast<uint64>(parameter.Descriptor.RegisterSpace));
				break;
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			default:
				hashValue = platformHashCombine(hashValue, 0);
				break;
			}
		}

		return hashValue;
	}
};
