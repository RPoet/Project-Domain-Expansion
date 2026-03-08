#include "Render/RenderWorld.h"

#include "Bridge/EntityBridge.h"
#include "Bridge/MeshBridge.h"
#include "Engine/Module/Asset/MeshStreaming.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Module/UI/ImGuiLayerModule.h"
#include "Render/RenderCommand.h"

class MeshCommandBuilder
{
private:
	RenderWorldBuildResult buildResult = {};

public:
	RenderWorldBuildResult build()
	{
		for (uint32 meshSlotIndex = 0; meshSlotIndex < MeshBridge::maxObjectCount; ++meshSlotIndex)
		{
			const MeshBridge::PackedHandle meshHandle = MeshBridge::get().getPackedHandleBySlotIndex(meshSlotIndex);
			if (meshHandle == MeshBridge::invalidPackedHandle)
			{
				continue;
			}

			const MeshBridge::StaticData* meshStaticData = MeshBridge::get().getStaticData(meshHandle);
			const MeshBridge::DynamicData* meshDynamicData = MeshBridge::get().getDynamicData(meshHandle);
			if (meshStaticData == nullptr
				|| meshDynamicData == nullptr
				|| !meshDynamicData->visible
				|| meshStaticData->entityHandle == invalidBridgeHandle
				|| meshStaticData->meshAssetHandle == nullptr
				|| meshStaticData->meshAssetHandle->state != MeshAssetHandleState::ready
				|| meshStaticData->meshAssetHandle->gpuState != MeshAssetGpuState::ready
				|| meshStaticData->meshAssetHandle->meshAsset == nullptr
				|| meshStaticData->meshAssetHandle->indexBufferObject == nullptr
				|| meshStaticData->meshAssetHandle->indexBufferSizeInBytes == 0)
			{
				continue;
			}

			const EntityBridge::DynamicData* entityDynamicData = EntityBridge::get().getDynamicData(meshStaticData->entityHandle);
			if (entityDynamicData == nullptr || !entityDynamicData->hasTransform)
			{
				continue;
			}

			RenderWorldMeshDrawData meshDrawData = {};
			meshDrawData.meshAssetHandle = meshStaticData->meshAssetHandle;
			meshDrawData.transform = entityDynamicData->transform;
			buildResult.meshDrawData.push_back(meshDrawData);
		}

		return moveValue(buildResult);
	}
};

class MeshDrawCommandBuilder
{
private:
	RenderWorldDrawPrepareResult drawPrepareResult = {};

public:
	RenderWorldDrawPrepareResult build(const RenderWorldBuildResult& buildResult)
	{
		for (uint32 meshDrawDataIndex = 0; meshDrawDataIndex < static_cast<uint32>(buildResult.meshDrawData.size()); ++meshDrawDataIndex)
		{
			const RenderWorldMeshDrawData& meshDrawData = buildResult.meshDrawData[meshDrawDataIndex];
			const shared_pointer<MeshAssetHandle>& meshAssetHandle = meshDrawData.meshAssetHandle;

			RenderWorldMeshDrawCommand meshDrawCommand = {};
			meshDrawCommand.meshAssetHandle = meshAssetHandle;
			meshDrawCommand.transform = meshDrawData.transform;
			meshDrawCommand.indexCount = meshAssetHandle->meshAsset->indexCount;
			meshDrawCommand.primitiveTopology = PrimitiveTopology::triangleList;
			meshDrawCommand.indexBufferBinding.resourceObject = meshAssetHandle->indexBufferObject.get();
			meshDrawCommand.indexBufferBinding.elementSize = IndexElementSize::thirtyTwoBits;
			meshDrawCommand.indexBufferBinding.sizeInBytes = meshAssetHandle->indexBufferSizeInBytes;
			meshDrawCommand.indexBufferBinding.offsetInBytes = 0;

			for (uint32 signatureIndex = 0; signatureIndex < meshVertexBufferSignatureCount; ++signatureIndex)
			{
				const MeshBufferSignature signature = static_cast<MeshBufferSignature>(signatureIndex);
				const uint32 signatureFlag = getMeshBufferSignatureFlag(signature);
				if ((meshAssetHandle->requiredVertexBufferFlags & signatureFlag) == 0)
				{
					continue;
				}

				BufferResourceObject* bufferObject = meshAssetHandle->getBufferObject(signature);
				if (bufferObject == nullptr)
				{
					meshDrawCommand.activeVertexBufferSlotFlags = 0;
					break;
				}

				VertexBufferBinding& vertexBufferBinding = meshDrawCommand.vertexBufferBindings[signatureIndex];
				vertexBufferBinding.resourceObject = bufferObject;
				vertexBufferBinding.strideInBytes = meshAssetHandle->getBufferStrideInBytes(signature);
				vertexBufferBinding.sizeInBytes = meshAssetHandle->getBufferSizeInBytes(signature);
				vertexBufferBinding.offsetInBytes = 0;
				meshDrawCommand.activeVertexBufferSlotFlags |= static_cast<uint32>(1u << signatureIndex);
			}

			if (meshDrawCommand.activeVertexBufferSlotFlags == 0
				|| meshDrawCommand.indexBufferBinding.resourceObject == nullptr
				|| meshDrawCommand.indexBufferBinding.sizeInBytes == 0
				|| meshDrawCommand.indexCount == 0)
			{
				continue;
			}

			bool validVertexBufferBinding = true;
			for (uint32 slotIndex = 0; slotIndex < renderBackendVertexBufferSlotCount; ++slotIndex)
			{
				if ((meshDrawCommand.activeVertexBufferSlotFlags & static_cast<uint32>(1u << slotIndex)) == 0)
				{
					continue;
				}

				const VertexBufferBinding& vertexBufferBinding = meshDrawCommand.vertexBufferBindings[slotIndex];
				if (vertexBufferBinding.resourceObject == nullptr
					|| vertexBufferBinding.strideInBytes == 0
					|| vertexBufferBinding.sizeInBytes == 0)
				{
					validVertexBufferBinding = false;
					break;
				}
			}

			if (!validVertexBufferBinding)
			{
				continue;
			}

			drawPrepareResult.meshDrawCommands.push_back(moveValue(meshDrawCommand));
		}

		return moveValue(drawPrepareResult);
	}
};

bool RenderWorld::initialize(WindowsWindowObject& windowObject)
{
	this->windowObject = &windowObject;
	consumedWorldUpdateSerial = 0;
	return true;
}

void RenderWorld::shutdown()
{
	windowObject = nullptr;
	consumedWorldUpdateSerial = 0;
}

bool RenderWorld::update(const RenderWorldUpdateInput& updateInput)
{
	assert(windowObject != nullptr);
	if (windowObject == nullptr)
	{
		return false;
	}

	if (!updateInput.worldFlow)
	{
		Renderer::flushRenderCommandQueue(updateInput.renderCommandFlushInput);
		return true;
	}

	if (updateInput.worldUpdateSerial == 0 || updateInput.worldUpdateSerial == consumedWorldUpdateSerial)
	{
		return true;
	}

	consumedWorldUpdateSerial = updateInput.worldUpdateSerial;
	MeshCommandBuilder builder = {};
	RenderWorldBuildResult buildResult = builder.build();
	MeshDrawCommandBuilder drawCommandBuilder = {};
	RenderWorldDrawPrepareResult drawPrepareResult = drawCommandBuilder.build(buildResult);
	unused(drawPrepareResult);

	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	if (renderBackendModule == nullptr
		|| !renderBackendModule->isBackendCreated()
		|| windowObject->isWindowMinimized())
	{
		return true;
	}

	RenderCommand& renderCommand = RenderCommand::get();
	renderCommand.enqueue("MeshUpload", [](string&& commandName, RenderBackend& renderBackendReference)
	{
		unused(commandName);
		MeshStreaming::get()->flushGpuRequests(renderBackendReference);
	});

	renderCommand.enqueue("Render", [](string&& commandName, RenderBackend& renderBackendReference)
	{
		unused(commandName);

		SwapChain* swapChain = renderBackendReference.getSwapChain();
		SyncObject* syncObject = renderBackendReference.getSyncObject();
		if (swapChain == nullptr || syncObject == nullptr)
		{
			return;
		}

		syncObject->wait();
		if (!swapChain->isRenderable())
		{
			return;
		}

		ResourceObject* outputResource = swapChain->getCurrentBackBufferResource();
		if (outputResource == nullptr)
		{
			renderBackendReference.releaseQueuedRenderResources();
			return;
		}

		RenderTargetView* renderTargetView = renderBackendReference.createRenderTargetView(outputResource);
		CommandList* commandList = renderBackendReference.acquireCommandList();
		if (renderTargetView == nullptr || commandList == nullptr)
		{
			if (commandList != nullptr)
			{
				renderBackendReference.releaseCommandList(commandList);
			}
			if (renderTargetView != nullptr)
			{
				renderBackendReference.destroyRenderTargetView(renderTargetView);
			}
			return;
		}

		commandList->reset();
		commandList->resourceBarrier(
			outputResource,
			ResourceState::present,
			ResourceState::renderTarget);
		commandList->setRenderTarget(renderTargetView);

		ViewportArea viewportArea = {};
		viewportArea.width = static_cast<float>(swapChain->getWidth());
		viewportArea.height = static_cast<float>(swapChain->getHeight());
		commandList->setViewport(viewportArea);

		ScissorRectArea scissorRectArea = {};
		scissorRectArea.right = static_cast<int32>(swapChain->getWidth());
		scissorRectArea.bottom = static_cast<int32>(swapChain->getHeight());
		commandList->setScissorRect(scissorRectArea);
		commandList->clearRenderTarget(
			renderTargetView,
			0.07f,
			0.11f,
			0.17f,
			1.0f);
		commandList->close();

		renderBackendReference.queueRenderTargetViewForDestroy(renderTargetView);
		renderBackendReference.queueCommandList(commandList);
	});

	renderCommand.enqueue("UI", [](string&& commandName, RenderBackend& renderBackendReference)
	{
		unused(commandName);

		shared_pointer<ImGuiLayerModule> imGuiLayerModule = ImGuiLayerModule::get();
		if (imGuiLayerModule == nullptr)
		{
			return;
		}

		SwapChain* swapChain = renderBackendReference.getSwapChain();
		if (swapChain == nullptr || !swapChain->isRenderable())
		{
			return;
		}

		ResourceObject* outputResource = swapChain->getCurrentBackBufferResource();
		if (outputResource == nullptr)
		{
			return;
		}

		RenderTargetView* renderTargetView = renderBackendReference.createRenderTargetView(outputResource);
		CommandList* commandList = renderBackendReference.acquireCommandList();
		if (renderTargetView == nullptr || commandList == nullptr)
		{
			if (commandList != nullptr)
			{
				renderBackendReference.releaseCommandList(commandList);
			}
			if (renderTargetView != nullptr)
			{
				renderBackendReference.destroyRenderTargetView(renderTargetView);
			}
			return;
		}

		commandList->reset();
		commandList->setRenderTarget(renderTargetView);
		imGuiLayerModule->buildAndRender(commandList);
		commandList->close();

		renderBackendReference.queueRenderTargetViewForDestroy(renderTargetView);
		renderBackendReference.queueCommandList(commandList);
	});

	renderCommand.enqueue("Present", [](string&& commandName, RenderBackend& renderBackendReference)
	{
		unused(commandName);

		CommandQueue* commandQueue = renderBackendReference.getCommandQueue();
		SwapChain* swapChain = renderBackendReference.getSwapChain();
		SyncObject* syncObject = renderBackendReference.getSyncObject();
		if (commandQueue == nullptr
			|| swapChain == nullptr
			|| syncObject == nullptr
			|| !swapChain->isRenderable())
		{
			renderBackendReference.releaseQueuedRenderResources();
			return;
		}

		ResourceObject* outputResource = swapChain->getCurrentBackBufferResource();
		if (outputResource == nullptr)
		{
			return;
		}

		CommandList* commandList = renderBackendReference.acquireCommandList();
		if (commandList == nullptr)
		{
			renderBackendReference.releaseQueuedRenderResources();
			return;
		}

		commandList->reset();
		commandList->resourceBarrier(
			outputResource,
			ResourceState::renderTarget,
			ResourceState::present);
		commandList->close();

		renderBackendReference.queueCommandList(commandList);
		renderBackendReference.executeQueuedCommandLists();
		swapChain->present();
		syncObject->signal();
		renderBackendReference.releaseQueuedRenderResources();
	});

	Renderer::flushRenderCommandQueue(updateInput.renderCommandFlushInput);
	return true;
}

RenderWorldBuildResult RenderWorld::build()
{
	MeshCommandBuilder builder = {};
	return builder.build();
}
