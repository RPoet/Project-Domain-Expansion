#pragma once

#include "Engine/Framework/Transform.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Engine/Window/WindowsWindowObject.h"
#include "Render/Backends/RenderBackendDefinitions.h"
#include "Render/CommandList.h"
#include "Render/DepthStencilView.h"
#include "Render/PipelineStateObject.h"
#include "Render/RenderCommand.h"
#include "Render/ResourceObject.h"

struct MeshAssetHandle;

struct RenderWorldUpdateInput
{
	bool worldFlow = false;
	uint64 worldUpdateSerial = 0;
	RenderCommand::RenderCommandFlushInput renderCommandFlushInput = {};
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
	PipelineStateDesc pipelineStateDesc = {};
	float baseColor[4] = {}; // <-- Debug Color
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

struct Temp_RenderWorldView
{
	unique_pointer<TextureResourceObject> depthTextureObject = nullptr;
	DepthStencilView* depthStencilView = nullptr;
	uint32 width = 0;
	uint32 height = 0;
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

	// TO DO : Change as signal
	uint64 consumedWorldUpdateSerial = 0;
	Temp_RenderWorldView view = {};
};
