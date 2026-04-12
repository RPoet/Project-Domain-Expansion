#pragma once

#include "Bridge/BridgeHandle.h"
#include "Bridge/DefaultBridge.h"
#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Platform/SIMDMath.h"

struct CameraBridgeObject
{
	struct StaticProperty
	{
		BridgeHandle entityHandle = invalidBridgeHandle;
		bool editorCamera = false;
	};

	struct DynamicProperty
	{
		bool primary = false;
		float fieldOfViewYDegrees = 60.0f;
		float nearPlane = 0.1f;
		float farPlane = 20000.0f;
		float4x4 viewMatrix = buildIdentityMatrix4x4();
	};

	struct ObjectDesc
	{
		StaticProperty staticProperty = {};
		DynamicProperty dynamicProperty = {};
	};
};

class CameraBridge final : public DefaultBridge<CameraBridgeObject>
{
public:
	CameraBridge(const CameraBridge&) = delete;
	CameraBridge& operator=(const CameraBridge&) = delete;
	CameraBridge(CameraBridge&&) = delete;
	CameraBridge& operator=(CameraBridge&&) = delete;

	static CameraBridge& get()
	{
		static CameraBridge cameraBridge;
		return cameraBridge;
	}

	HandleReference createCameraHandle(const ObjectDesc& objectDesc);
	bool isHandleAlive(PackedHandle packedHandle) const;
	const StaticData* getStaticData(PackedHandle packedHandle) const;
	const DynamicData* getDynamicData(PackedHandle packedHandle) const;
	void updateDynamicData(PackedHandle packedHandle, const DynamicData& dynamicData);
	void processFrame();

private:
	CameraBridge() = default;
	~CameraBridge() = default;
};
