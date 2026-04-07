#pragma once

#include "Bridge/BridgeHandle.h"
#include "Bridge/DefaultBridge.h"
#include "Engine/Assets/MaterialAsset.h"

struct MaterialBridgeObject
{
	struct StaticProperty
	{
		string materialAssetPath = {};
		shared_pointer<MaterialAsset> materialAsset = nullptr;
	};

	struct DynamicProperty
	{
	};

	struct ObjectDesc
	{
		StaticProperty staticProperty = {};
		DynamicProperty dynamicProperty = {};
	};
};

class MaterialBridge final : public DefaultBridge<MaterialBridgeObject>
{
public:
	MaterialBridge(const MaterialBridge&) = delete;
	MaterialBridge& operator=(const MaterialBridge&) = delete;
	MaterialBridge(MaterialBridge&&) = delete;
	MaterialBridge& operator=(MaterialBridge&&) = delete;

	static MaterialBridge& get()
	{
		static MaterialBridge materialBridge;
		return materialBridge;
	}

	HandleReference createMaterialHandle(const ObjectDesc& objectDesc);
	bool isHandleAlive(PackedHandle packedHandle) const;
	const StaticData* getStaticData(PackedHandle packedHandle) const;
	bool resolveEffectiveShaders(
		PackedHandle packedHandle,
		ShaderTargetPlatform targetPlatform,
		shared_pointer<ShaderObject>& outVertexShader,
		shared_pointer<ShaderObject>& outPixelShader) const;
	void processFrame();

private:
	MaterialBridge() = default;
	~MaterialBridge() = default;
};
