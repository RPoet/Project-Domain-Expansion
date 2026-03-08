#pragma once

#include "Engine/Framework/Transform.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Engine/Window/WindowsWindowObject.h"
#include "Render/Backends/RenderBackendDefinitions.h"
#include "Render/CommandList.h"
#include "Render/Renderer.h"

struct MeshAssetHandle;

struct RenderWorldUpdateInput
{
	bool worldFlow = false;
	uint64 worldUpdateSerial = 0;
	Renderer::RenderCommandFlushInput renderCommandFlushInput = {};
};

struct RenderWorldMeshDrawData
{
	shared_pointer<MeshAssetHandle> meshAssetHandle = nullptr;
	Transform transform = {};
};

struct RenderWorldBuildResult
{
	vector<RenderWorldMeshDrawData> meshDrawData = {};
};

struct RenderWorldMeshDrawCommand
{
	shared_pointer<MeshAssetHandle> meshAssetHandle = nullptr;
	Transform transform = {};
	PrimitiveTopology primitiveTopology = PrimitiveTopology::triangleList;
	VertexBufferBinding vertexBufferBindings[renderBackendVertexBufferSlotCount] = {};
	uint32 activeVertexBufferSlotFlags = 0;
	IndexBufferBinding indexBufferBinding = {};
	uint32 indexCount = 0;
};

struct RenderWorldDrawPrepareResult
{
	vector<RenderWorldMeshDrawCommand> meshDrawCommands = {};
};

class RenderWorld
{
public:
	bool initialize(WindowsWindowObject& windowObject);
	void shutdown();
	bool update(const RenderWorldUpdateInput& updateInput);
	RenderWorldBuildResult build();

private:
	WindowsWindowObject* windowObject = nullptr;
	uint64 consumedWorldUpdateSerial = 0;
};
