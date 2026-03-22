#include "Render/RenderWorld.h"

#include "Bridge/CameraBridge.h"
#include "Bridge/EntityBridge.h"
#include "Bridge/MeshBridge.h"
#include "Engine/Module/MeshStreaming/MeshStreaming.h"
#include "Engine/Module/ShaderPackage/ShaderPackageModule.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Module/UI/ImGuiLayerModule.h"
#include "Render/RenderCommand.h"
#include "Render/Renderer.h"

class RenderCameraBuilder
{
public:
	vector<BridgeHandle> build()
	{
		vector<BridgeHandle> cameraHandles = {};

		for (uint32 cameraSlotIndex = 0; cameraSlotIndex < CameraBridge::maxObjectCount; ++cameraSlotIndex)
		{
			const CameraBridge::PackedHandle cameraHandle = CameraBridge::get().getPackedHandleBySlotIndex(cameraSlotIndex);
			if (cameraHandle == CameraBridge::invalidPackedHandle)
			{
				continue;
			}

			const CameraBridge::StaticData* cameraStaticData = CameraBridge::get().getStaticData(cameraHandle);
			if (cameraStaticData == nullptr
				|| cameraStaticData->entityHandle == invalidBridgeHandle)
			{
				continue;
			}

			assert(CameraBridge::get().getDynamicData(cameraHandle) != nullptr && "[RenderWorld][Assert] reason=camera_dynamic_data_missing");

			const EntityBridge::DynamicData* entityDynamicData = EntityBridge::get().getDynamicData(cameraStaticData->entityHandle);
			if (entityDynamicData == nullptr || !entityDynamicData->active)
			{
				continue;
			}

			cameraHandles.push_back(cameraHandle);
		}

		return cameraHandles;
	}
};

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
			if (entityDynamicData == nullptr || !entityDynamicData->active || !entityDynamicData->hasTransform)
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
		shared_pointer<ShaderPackageModule> shaderPackageModule = ShaderPackageModule::get();
		if (shaderPackageModule == nullptr)
		{
			return moveValue(drawPrepareResult);
		}

		for (uint32 meshDrawDataIndex = 0; meshDrawDataIndex < static_cast<uint32>(buildResult.meshDrawData.size()); ++meshDrawDataIndex)
		{
			const RenderWorldMeshDrawData& meshDrawData = buildResult.meshDrawData[meshDrawDataIndex];
			const shared_pointer<MeshAssetHandle>& meshAssetHandle = meshDrawData.meshAssetHandle;
			shared_pointer<ShaderPackageAsset> shaderPackage = shaderPackageModule->getOrLoadPackage("Shaders/Packages/GeometryBaseColor.shaderpkg");
			if (shaderPackage == nullptr || shaderPackage->state != ShaderPackageState::ready)
			{
				continue;
			}

			const ShaderPackageVariant* shaderVariant = nullptr;
			for (uint32 variantIndex = 0; variantIndex < static_cast<uint32>(shaderPackage->variants.size()); ++variantIndex)
			{
				const ShaderPackageVariant& currentVariant = shaderPackage->variants[variantIndex];
				if (currentVariant.name == "GeometryDefault")
				{
					shaderVariant = &currentVariant;
					break;
				}
			}

			if (shaderVariant == nullptr)
			{
				continue;
			}

			shared_pointer<ShaderObject> vertexShader = shaderVariant->getShader(ShaderStage::vertex);
			shared_pointer<ShaderObject> pixelShader = shaderVariant->getShader(ShaderStage::pixel);
			if (vertexShader == nullptr || pixelShader == nullptr)
			{
				continue;
			}

			RenderWorldMeshDrawCommand meshDrawCommand = {};
			meshDrawCommand.meshAssetHandle = meshAssetHandle;
			meshDrawCommand.transform = meshDrawData.transform;
			meshDrawCommand.pipelineStateDesc.pipelineStateType = PipelineStateType::graphics;
			meshDrawCommand.pipelineStateDesc.vertexShader = vertexShader;
			meshDrawCommand.pipelineStateDesc.pixelShader = pixelShader;
			PushConstantRange pushConstantRange = {};
			pushConstantRange.offsetInBytes = 0;
			pushConstantRange.sizeInBytes = static_cast<uint32>(sizeof(float) * 20);
			pushConstantRange.shaderVisibility = ShaderVisibility::allGraphics;
			meshDrawCommand.pipelineStateDesc.rootSignatureDesc.pushConstantRanges.push_back(pushConstantRange);

			PipelineInputElementDesc positionInputElement = {};
			positionInputElement.semantic = VertexInputSemantic::position;
			positionInputElement.format = VertexInputFormat::float3;
			positionInputElement.inputSlot = getMeshBufferSignatureIndex(MeshBufferSignature::position);
			meshDrawCommand.pipelineStateDesc.inputElements.push_back(positionInputElement);

			PipelineInputElementDesc normalInputElement = {};
			normalInputElement.semantic = VertexInputSemantic::normal;
			normalInputElement.format = VertexInputFormat::float3;
			normalInputElement.inputSlot = getMeshBufferSignatureIndex(MeshBufferSignature::normal);
			meshDrawCommand.pipelineStateDesc.inputElements.push_back(normalInputElement);

			PipelineInputElementDesc texcoordInputElement = {};
			texcoordInputElement.semantic = VertexInputSemantic::texcoord;
			texcoordInputElement.format = VertexInputFormat::float2;
			texcoordInputElement.inputSlot = getMeshBufferSignatureIndex(MeshBufferSignature::texcoord);
			meshDrawCommand.pipelineStateDesc.inputElements.push_back(texcoordInputElement);

			PipelineRenderTargetDesc renderTargetDesc = {};
			renderTargetDesc.colorFormat = TextureFormat::rgba8Unorm;
			meshDrawCommand.pipelineStateDesc.renderTargets.push_back(renderTargetDesc);
			meshDrawCommand.pipelineStateDesc.depthStencilDesc.depthStencilFormat = TextureFormat::d32Float;
			meshDrawCommand.pipelineStateDesc.depthStencilDesc.depthTestEnabled = true;
			meshDrawCommand.pipelineStateDesc.depthStencilDesc.depthWriteEnabled = true;
			meshDrawCommand.pipelineStateDesc.depthStencilDesc.depthCompareOperation = PipelineCompareOperation::lessEqual;
			meshDrawCommand.pipelineStateDesc.cullMode = PipelineCullMode::back;
			meshDrawCommand.pipelineStateDesc.sampleCount = 1;
			meshDrawCommand.baseColor[0] = 0.86f;
			meshDrawCommand.baseColor[1] = 0.73f;
			meshDrawCommand.baseColor[2] = 0.42f;
			meshDrawCommand.baseColor[3] = 1.0f;
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
	view.depthTextureObject.reset();
	view.depthStencilView = nullptr;
	view.width = 0;
	view.height = 0;
	return true;
}

void RenderWorld::shutdown()
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	RenderBackend* renderBackend = renderBackendModule != nullptr ? renderBackendModule->getBackend() : nullptr;
	if (renderBackend != nullptr && view.depthStencilView != nullptr)
	{
		renderBackend->destroyDepthStencilView(view.depthStencilView);
	}

	view.depthTextureObject.reset();
	view.depthStencilView = nullptr;
	view.width = 0;
	view.height = 0;
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
		RenderCommand::flushRenderCommandQueue(updateInput.renderCommandFlushInput);
		return true;
	}

	if (updateInput.worldUpdateSerial == 0 || updateInput.worldUpdateSerial == consumedWorldUpdateSerial)
	{
		return true;
	}

	consumedWorldUpdateSerial = updateInput.worldUpdateSerial;
	MeshCommandBuilder meshBuilder = {};
	RenderWorldBuildResult buildResult = meshBuilder.build();
	RenderCameraBuilder cameraBuilder = {};
	buildResult.cameraHandles = cameraBuilder.build();
	MeshDrawCommandBuilder drawCommandBuilder = {};
	RenderWorldDrawPrepareResult drawPrepareResult = drawCommandBuilder.build(buildResult);

	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	if (renderBackendModule == nullptr
		|| !renderBackendModule->isBackendCreated()
		|| windowObject->isWindowMinimized())
	{
		return true;
	}

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	if (renderBackend == nullptr)
	{
		return true;
	}

	SwapChain* swapChain = renderBackend->getSwapChain();
	if (swapChain == nullptr || !swapChain->isRenderable())
	{
		return true;
	}

	const uint32 swapChainWidth = swapChain->getWidth();
	const uint32 swapChainHeight = swapChain->getHeight();
	if (swapChainWidth == 0 || swapChainHeight == 0)
	{
		return true;
	}

	const bool depthBufferResizeRequired =
		view.depthTextureObject == nullptr
		|| view.depthStencilView == nullptr
		|| view.width != swapChainWidth
		|| view.height != swapChainHeight;
	if (depthBufferResizeRequired)
	{
		SyncObject* syncObject = renderBackend->getSyncObject();
		if (syncObject != nullptr)
		{
			syncObject->wait();
		}

		if (view.depthStencilView != nullptr)
		{
			renderBackend->destroyDepthStencilView(view.depthStencilView);
			view.depthStencilView = nullptr;
		}

		view.depthTextureObject.reset();
		view.width = 0;
		view.height = 0;

		TextureObjectCreateOptions depthTextureCreateOptions = {};
		depthTextureCreateOptions.width = swapChainWidth;
		depthTextureCreateOptions.height = swapChainHeight;
		depthTextureCreateOptions.format = TextureFormat::d32Float;
		depthTextureCreateOptions.flags = getTextureObjectFlag(TextureObjectFlag::allowDepthStencil);
		depthTextureCreateOptions.initialState = ResourceState::depthWrite;
		depthTextureCreateOptions.sampleCount = 1;
		depthTextureCreateOptions.clearDepth = 1.0f;
		depthTextureCreateOptions.clearStencil = 0;
		view.depthTextureObject = renderBackend->createTextureObject(depthTextureCreateOptions);
		if (view.depthTextureObject != nullptr)
		{
			view.depthStencilView = renderBackend->createDepthStencilView(view.depthTextureObject.get());
		}

		if (view.depthTextureObject == nullptr || view.depthStencilView == nullptr)
		{
			if (view.depthStencilView != nullptr)
			{
				renderBackend->destroyDepthStencilView(view.depthStencilView);
				view.depthStencilView = nullptr;
			}

			view.depthTextureObject.reset();
			error << "[RenderWorld][Error] reason=depth_resource_create_failed" << lineBreak;
			return true;
		}

		view.width = swapChainWidth;
		view.height = swapChainHeight;
	}

	RenderCommand& renderCommand = RenderCommand::get();
	shared_pointer<RenderWorldDrawPrepareResult> drawPrepareResultHandle(new RenderWorldDrawPrepareResult(moveValue(drawPrepareResult)));
	const BridgeHandle cameraHandle =
		!buildResult.cameraHandles.empty()
		? buildResult.cameraHandles[0]
		: invalidBridgeHandle;
	renderCommand.enqueue("MeshUpload", [](string&& commandName, RenderBackend& renderBackendReference)
	{
		unused(commandName);
		MeshStreaming::get()->flushGpuRequests(renderBackendReference);
	});

	renderCommand.enqueue("Render", [this, drawPrepareResultHandle, cameraHandle](string&& commandName, RenderBackend& renderBackendReference)
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

		TextureResourceObject* outputResource = swapChain->getCurrentBackBufferResource();
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
		RenderTargetView* renderTargetViews[1] = { renderTargetView };
		commandList->setRenderTargets(renderTargetViews, 1, view.depthStencilView);

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
		commandList->clearDepthStencil(view.depthStencilView, 1.0f, 0);
		if (drawPrepareResultHandle != nullptr && cameraHandle != invalidBridgeHandle)
		{
			Renderer renderer = {};
			renderer.setBackend(&renderBackendReference);
			const CameraBridge::DynamicData* cameraDynamicData = CameraBridge::get().getDynamicData(cameraHandle);
			assert(cameraDynamicData != nullptr && "[RenderWorld][Assert] reason=camera_dynamic_data_missing");

			const float4x4 viewProjectionMatrix = multiplyMatrix4x4(
				cameraDynamicData->viewMatrix,
				buildProjectionMatrix4x4(
					swapChain->getWidth(),
					swapChain->getHeight(),
					cameraDynamicData->fieldOfViewYDegrees,
					cameraDynamicData->nearPlane,
					cameraDynamicData->farPlane));
			renderer.drawGeometry(
				commandList,
				*drawPrepareResultHandle,
				viewProjectionMatrix);
		}
		commandList->close();

		renderBackendReference.queueRenderTargetViewForDestroy(renderTargetView);
		renderBackendReference.queueCommandList(commandList);
	});

	renderCommand.enqueue("UI", [this, cameraHandle](string&& commandName, RenderBackend& renderBackendReference)
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

		TextureResourceObject* outputResource = swapChain->getCurrentBackBufferResource();
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
		RenderTargetView* renderTargetViews[1] = { renderTargetView };
		commandList->setRenderTargets(renderTargetViews, 1, view.depthStencilView);

		ViewportArea viewportArea = {};
		viewportArea.width = static_cast<float>(swapChain->getWidth());
		viewportArea.height = static_cast<float>(swapChain->getHeight());
		commandList->setViewport(viewportArea);

		ScissorRectArea scissorRectArea = {};
		scissorRectArea.right = static_cast<int32>(swapChain->getWidth());
		scissorRectArea.bottom = static_cast<int32>(swapChain->getHeight());
		commandList->setScissorRect(scissorRectArea);

		float4x4 editorViewProjectionMatrix = {};
		const float4x4* editorViewProjectionMatrixPointer = nullptr;
		if (cameraHandle != invalidBridgeHandle)
		{
			const CameraBridge::DynamicData* cameraDynamicData = CameraBridge::get().getDynamicData(cameraHandle);
			if (cameraDynamicData != nullptr)
			{
				editorViewProjectionMatrix = multiplyMatrix4x4(
					cameraDynamicData->viewMatrix,
					buildProjectionMatrix4x4(
						swapChain->getWidth(),
						swapChain->getHeight(),
						cameraDynamicData->fieldOfViewYDegrees,
						cameraDynamicData->nearPlane,
						cameraDynamicData->farPlane));
				editorViewProjectionMatrixPointer = &editorViewProjectionMatrix;
			}
		}

		imGuiLayerModule->buildAndRender(commandList, editorViewProjectionMatrixPointer);
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

		TextureResourceObject* outputResource = swapChain->getCurrentBackBufferResource();
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

	RenderCommand::flushRenderCommandQueue(updateInput.renderCommandFlushInput);
	return true;
}

RenderWorldBuildResult RenderWorld::build()
{
	MeshCommandBuilder meshBuilder = {};
	RenderWorldBuildResult buildResult = meshBuilder.build();
	RenderCameraBuilder cameraBuilder = {};
	buildResult.cameraHandles = cameraBuilder.build();
	return buildResult;
}
