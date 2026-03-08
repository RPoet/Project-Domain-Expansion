#pragma once

#include "Engine/Framework/Transform.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Engine/Window/WindowsWindowObject.h"
#include "Render/Renderer.h"

struct MeshAssetHandle;

class RenderWorld
{
public:
	struct UpdateInput
	{
		bool worldFlow = false;
		uint64 worldUpdateSerial = 0;
		Renderer::RenderCommandFlushInput renderCommandFlushInput = {};
	};

	struct MeshDrawData
	{
		shared_pointer<MeshAssetHandle> meshAssetHandle = nullptr;
		Transform transform = {};
	};

	struct BuildResult
	{
		vector<MeshDrawData> meshDrawData = {};
	};

	bool initialize(WindowsWindowObject& windowObject);
	void shutdown();
	bool update(const UpdateInput& updateInput);
	BuildResult build();

private:
	WindowsWindowObject* windowObject = nullptr;
	uint64 consumedWorldUpdateSerial = 0;
};
