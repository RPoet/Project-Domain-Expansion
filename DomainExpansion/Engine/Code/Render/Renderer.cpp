#include "Render/Renderer.h"

#include "Engine/Module/Asset/MeshStreaming.h"
#include "Render/RenderWorld.h"

struct Temp_GeometryPushConstantData
{
	float worldViewProjection[16] = {};
	float baseColor[4] = {};
};

void Renderer::setBackend(RenderBackend* renderBackend)
{
	this->renderBackend = renderBackend;
}

void Renderer::drawGeometry(
	CommandList* commandList,
	const RenderWorldDrawPrepareResult& drawPrepareResult,
	const float4x4& viewProjectionMatrix)
{
	if (renderBackend == nullptr
		|| commandList == nullptr
		|| drawPrepareResult.meshDrawCommands.empty())
	{
		return;
	}
	PipelineStateObject* currentPipelineStateObject = nullptr;
	RootSignatureObject* currentRootSignatureObject = nullptr;

	for (uint32 drawCommandIndex = 0; drawCommandIndex < static_cast<uint32>(drawPrepareResult.meshDrawCommands.size()); ++drawCommandIndex)
	{
		const RenderWorldMeshDrawCommand& meshDrawCommand = drawPrepareResult.meshDrawCommands[drawCommandIndex];
		if (meshDrawCommand.indexCount == 0
			|| meshDrawCommand.activeVertexBufferSlotFlags == 0
			|| meshDrawCommand.pipelineStateDesc.vertexShader == nullptr
			|| meshDrawCommand.pipelineStateDesc.pixelShader == nullptr)
		{
			continue;
		}

		RootSignatureObject* rootSignatureObject = renderBackend->getOrCreateRootSignatureObject(meshDrawCommand.pipelineStateDesc.rootSignatureDesc);
		if (rootSignatureObject == nullptr)
		{
			continue;
		}

		PipelineStateObject* pipelineStateObject = renderBackend->getOrCreatePipelineStateObject(meshDrawCommand.pipelineStateDesc);
		if (pipelineStateObject == nullptr)
		{
			continue;
		}

		if (pipelineStateObject != currentPipelineStateObject || rootSignatureObject != currentRootSignatureObject)
		{
			commandList->setPipeline(pipelineStateObject, rootSignatureObject);
			currentPipelineStateObject = pipelineStateObject;
			currentRootSignatureObject = rootSignatureObject;
		}

		float3 position = {};
		position.x = meshDrawCommand.transform.positionX;
		position.y = meshDrawCommand.transform.positionY;
		position.z = meshDrawCommand.transform.positionZ;

		float3 rotation = {};
		rotation.x = meshDrawCommand.transform.rotationPitch;
		rotation.y = meshDrawCommand.transform.rotationYaw;
		rotation.z = meshDrawCommand.transform.rotationRoll;

		float3 scale = {};
		scale.x = meshDrawCommand.transform.scaleX;
		scale.y = meshDrawCommand.transform.scaleY;
		scale.z = meshDrawCommand.transform.scaleZ;

		const float4x4 worldViewProjectionMatrix = multiplyMatrix4x4(
			buildWorldMatrix4x4(position, rotation, scale),
			viewProjectionMatrix);
		Temp_GeometryPushConstantData pushConstantData = {};
		for (uint32 matrixIndex = 0; matrixIndex < 16; ++matrixIndex)
		{
			pushConstantData.worldViewProjection[matrixIndex] = worldViewProjectionMatrix.value[matrixIndex];
		}
		pushConstantData.baseColor[0] = meshDrawCommand.baseColor[0];
		pushConstantData.baseColor[1] = meshDrawCommand.baseColor[1];
		pushConstantData.baseColor[2] = meshDrawCommand.baseColor[2];
		pushConstantData.baseColor[3] = meshDrawCommand.baseColor[3];

		commandList->setGraphicsPushConstants(0, &pushConstantData, static_cast<uint32>(sizeof(pushConstantData)));
		commandList->setPrimitiveTopology(meshDrawCommand.primitiveTopology);
		for (uint32 slotIndex = 0; slotIndex < renderBackendVertexBufferSlotCount; ++slotIndex)
		{
			if ((meshDrawCommand.activeVertexBufferSlotFlags & static_cast<uint32>(1u << slotIndex)) == 0)
			{
				continue;
			}

			commandList->setVertexBuffer(slotIndex, meshDrawCommand.vertexBufferBindings[slotIndex]);
		}
		commandList->setIndexBuffer(meshDrawCommand.indexBufferBinding);
		commandList->drawIndexed(meshDrawCommand.indexCount, 1, 0, 0, 0);
	}
}

void Renderer::render(CommandList* commandList)
{
	if (renderBackend == nullptr)
	{
		return;
	}

	SyncObject* syncObject = renderBackend->getSyncObject();
	CommandQueue* commandQueue = renderBackend->getCommandQueue();
	SwapChain* swapChain = renderBackend->getSwapChain();
	if (syncObject == nullptr || commandQueue == nullptr || commandList == nullptr || swapChain == nullptr)
	{
		return;
	}

	syncObject->wait();
	if (!swapChain->isRenderable())
	{
		return;
	}

	TextureResourceObject* backBufferResource = swapChain->getCurrentBackBufferResource();
	RenderTargetView* backBufferView = renderBackend->createRenderTargetView(backBufferResource);
	if (backBufferResource == nullptr || backBufferView == nullptr)
	{
		return;
	}

	outputResource = backBufferResource;

	commandList->reset();
	commandList->resourceBarrier(
		backBufferResource,
		ResourceState::present,
		ResourceState::renderTarget);
	RenderTargetView* renderTargetViews[1] = { backBufferView };
	commandList->setRenderTargets(renderTargetViews, 1, nullptr);

	ViewportArea viewportArea = {};
	viewportArea.width = static_cast<float>(swapChain->getWidth());
	viewportArea.height = static_cast<float>(swapChain->getHeight());
	commandList->setViewport(viewportArea);

	ScissorRectArea scissorRectArea = {};
	scissorRectArea.right = static_cast<int32>(swapChain->getWidth());
	scissorRectArea.bottom = static_cast<int32>(swapChain->getHeight());
	commandList->setScissorRect(scissorRectArea);

	commandList->clearRenderTarget(
		backBufferView,
		clearColor.red,
		clearColor.green,
		clearColor.blue,
		clearColor.alpha);

	commandList->resourceBarrier(
		backBufferResource,
		ResourceState::renderTarget,
		ResourceState::present);
	commandList->close();
	commandQueue->execute(commandList);

	renderBackend->destroyRenderTargetView(backBufferView);
}

ResourceObject* Renderer::getOutputResource() const
{
	return outputResource;
}
