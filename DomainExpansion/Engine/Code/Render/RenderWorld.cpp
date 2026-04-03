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

#include <cmath>

static float computeRenderWorldFractionalPart(const float value)
{
	return value - floorf(value);
}

static void convertRenderWorldHSVToRGB(
	const float hue,
	const float saturation,
	const float value,
	float& outRed,
	float& outGreen,
	float& outBlue)
{
	const float wrappedHue = computeRenderWorldFractionalPart(hue) * 6.0f;
	const int32 hueSector = static_cast<int32>(wrappedHue);
	const float hueFraction = wrappedHue - static_cast<float>(hueSector);
	const float p = value * (1.0f - saturation);
	const float q = value * (1.0f - (saturation * hueFraction));
	const float t = value * (1.0f - (saturation * (1.0f - hueFraction)));

	switch (hueSector % 6)
	{
	case 0:
		outRed = value;
		outGreen = t;
		outBlue = p;
		return;
	case 1:
		outRed = q;
		outGreen = value;
		outBlue = p;
		return;
	case 2:
		outRed = p;
		outGreen = value;
		outBlue = t;
		return;
	case 3:
		outRed = p;
		outGreen = q;
		outBlue = value;
		return;
	case 4:
		outRed = t;
		outGreen = p;
		outBlue = value;
		return;
	default:
		outRed = value;
		outGreen = p;
		outBlue = q;
		return;
	}
}

static void buildRenderWorldSectionDebugColor(
	const uint32 meshDrawDataIndex,
	const uint32 sectionIndex,
	float outColor[4])
{
	const uint32 meshSeed = (meshDrawDataIndex + 1u) * 0x9E3779B9u;
	const uint32 sectionSeed = (sectionIndex + 1u) * 0x85EBCA6Bu;
	const uint32 combinedSeed = meshSeed ^ sectionSeed ^ 0xC2B2AE35u;
	const float hue = computeRenderWorldFractionalPart(
		(static_cast<float>(combinedSeed & 0xFFFFu) / 65535.0f)
		+ (static_cast<float>(meshDrawDataIndex) * 0.173f)
		+ (static_cast<float>(sectionIndex) * 0.327f));
	const float saturation = 0.65f + (static_cast<float>((combinedSeed >> 8) & 0xFFu) / 255.0f) * 0.25f;
	const float value = 0.78f + (static_cast<float>((combinedSeed >> 16) & 0xFFu) / 255.0f) * 0.18f;
	convertRenderWorldHSVToRGB(hue, saturation, value, outColor[0], outColor[1], outColor[2]);
	outColor[3] = 1.0f;
}

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

		shared_pointer<ShaderPackageAsset> shaderPackage = shaderPackageModule->getOrLoadPackage("Shaders/Packages/GeometryBaseColor.shaderpkg");
		if (shaderPackage == nullptr || shaderPackage->state != ShaderPackageState::ready)
		{
			return moveValue(drawPrepareResult);
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
			return moveValue(drawPrepareResult);
		}

		shared_pointer<ShaderObject> vertexShader = shaderVariant->getShader(ShaderStage::vertex);
		shared_pointer<ShaderObject> pixelShader = shaderVariant->getShader(ShaderStage::pixel);
		if (vertexShader == nullptr || pixelShader == nullptr)
		{
			return moveValue(drawPrepareResult);
		}

		for (uint32 meshDrawDataIndex = 0; meshDrawDataIndex < static_cast<uint32>(buildResult.meshDrawData.size()); ++meshDrawDataIndex)
		{
			const RenderWorldMeshDrawData& meshDrawData = buildResult.meshDrawData[meshDrawDataIndex];
			const shared_pointer<MeshAssetHandle>& meshAssetHandle = meshDrawData.meshAssetHandle;
			const uint32 lodLevel = meshAssetHandle->lodLevel;
			const uint32 totalIndexCount = meshAssetHandle->meshAsset->getIndexCount(lodLevel);
			if (totalIndexCount == 0)
			{
				continue;
			}

			RenderWorldMeshDrawCommand baseMeshDrawCommand = {};
			baseMeshDrawCommand.meshAssetHandle = meshAssetHandle;
			baseMeshDrawCommand.transform = meshDrawData.transform;
			baseMeshDrawCommand.pipelineStateDesc.pipelineStateType = PipelineStateType::graphics;
			baseMeshDrawCommand.pipelineStateDesc.vertexShader = vertexShader;
			baseMeshDrawCommand.pipelineStateDesc.pixelShader = pixelShader;
			PushConstantRange pushConstantRange = {};
			pushConstantRange.offsetInBytes = 0;
			pushConstantRange.sizeInBytes = static_cast<uint32>(sizeof(float) * 20);
			pushConstantRange.shaderVisibility = ShaderVisibility::allGraphics;
			baseMeshDrawCommand.pipelineStateDesc.rootSignatureDesc.pushConstantRanges.push_back(pushConstantRange);

			PipelineInputElementDesc positionInputElement = {};
			positionInputElement.semantic = VertexInputSemantic::position;
			positionInputElement.format = VertexInputFormat::float3;
			positionInputElement.inputSlot = getMeshBufferSignatureIndex(MeshBufferSignature::position);
			baseMeshDrawCommand.pipelineStateDesc.inputElements.push_back(positionInputElement);

			PipelineInputElementDesc normalInputElement = {};
			normalInputElement.semantic = VertexInputSemantic::normal;
			normalInputElement.format = VertexInputFormat::float3;
			normalInputElement.inputSlot = getMeshBufferSignatureIndex(MeshBufferSignature::normal);
			baseMeshDrawCommand.pipelineStateDesc.inputElements.push_back(normalInputElement);

			PipelineInputElementDesc texcoordInputElement = {};
			texcoordInputElement.semantic = VertexInputSemantic::texcoord;
			texcoordInputElement.format = VertexInputFormat::float2;
			texcoordInputElement.inputSlot = getMeshBufferSignatureIndex(MeshBufferSignature::texcoord);
			baseMeshDrawCommand.pipelineStateDesc.inputElements.push_back(texcoordInputElement);

			PipelineRenderTargetDesc renderTargetDesc = {};
			renderTargetDesc.colorFormat = TextureFormat::rgba8Unorm;
			baseMeshDrawCommand.pipelineStateDesc.renderTargets.push_back(renderTargetDesc);
			baseMeshDrawCommand.pipelineStateDesc.depthStencilDesc.depthStencilFormat = TextureFormat::d32Float;
			baseMeshDrawCommand.pipelineStateDesc.depthStencilDesc.depthTestEnabled = true;
			baseMeshDrawCommand.pipelineStateDesc.depthStencilDesc.depthWriteEnabled = true;
			baseMeshDrawCommand.pipelineStateDesc.depthStencilDesc.depthCompareOperation = PipelineCompareOperation::lessEqual;
			baseMeshDrawCommand.pipelineStateDesc.cullMode = PipelineCullMode::back;
			baseMeshDrawCommand.pipelineStateDesc.sampleCount = 1;
			baseMeshDrawCommand.primitiveTopology = PrimitiveTopology::triangleList;
			baseMeshDrawCommand.indexBufferBinding.resourceObject = meshAssetHandle->indexBufferObject.get();
			baseMeshDrawCommand.indexBufferBinding.elementSize = IndexElementSize::thirtyTwoBits;
			baseMeshDrawCommand.indexBufferBinding.sizeInBytes = meshAssetHandle->indexBufferSizeInBytes;
			baseMeshDrawCommand.indexBufferBinding.offsetInBytes = 0;

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
					baseMeshDrawCommand.activeVertexBufferSlotFlags = 0;
					break;
				}

				VertexBufferBinding& vertexBufferBinding = baseMeshDrawCommand.vertexBufferBindings[signatureIndex];
				vertexBufferBinding.resourceObject = bufferObject;
				vertexBufferBinding.strideInBytes = meshAssetHandle->getBufferStrideInBytes(signature);
				vertexBufferBinding.sizeInBytes = meshAssetHandle->getBufferSizeInBytes(signature);
				vertexBufferBinding.offsetInBytes = 0;
				baseMeshDrawCommand.activeVertexBufferSlotFlags |= static_cast<uint32>(1u << signatureIndex);
			}

			if (baseMeshDrawCommand.activeVertexBufferSlotFlags == 0
				|| baseMeshDrawCommand.indexBufferBinding.resourceObject == nullptr
				|| baseMeshDrawCommand.indexBufferBinding.sizeInBytes == 0)
			{
				continue;
			}

			bool validVertexBufferBinding = true;
			for (uint32 slotIndex = 0; slotIndex < renderBackendVertexBufferSlotCount; ++slotIndex)
			{
				if ((baseMeshDrawCommand.activeVertexBufferSlotFlags & static_cast<uint32>(1u << slotIndex)) == 0)
				{
					continue;
				}

				const VertexBufferBinding& vertexBufferBinding = baseMeshDrawCommand.vertexBufferBindings[slotIndex];
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

			const vector<RawMeshData::MeshSectionRange>& sectionRanges = meshAssetHandle->meshAsset->getSectionRanges(lodLevel);
			if (sectionRanges.empty())
			{
				assert(false && "[RenderWorld][Assert] reason=mesh_section_ranges_missing");
				continue;
			}

			for (uint32 sectionIndex = 0; sectionIndex < static_cast<uint32>(sectionRanges.size()); ++sectionIndex)
			{
				const RawMeshData::MeshSectionRange& sectionRange = sectionRanges[sectionIndex];
				const bool validSectionRange =
					sectionRange.indexCount > 0
					&& sectionRange.startIndex < totalIndexCount
					&& sectionRange.startIndex + sectionRange.indexCount <= totalIndexCount;
				if (!validSectionRange)
				{
					continue;
				}

				RenderWorldMeshDrawCommand meshDrawCommand = baseMeshDrawCommand;
				buildRenderWorldSectionDebugColor(meshDrawDataIndex, sectionIndex, meshDrawCommand.baseColor);
				meshDrawCommand.indexCount = sectionRange.indexCount;
				meshDrawCommand.startIndexLocation = sectionRange.startIndex;
				drawPrepareResult.meshDrawCommands.push_back(moveValue(meshDrawCommand));
			}
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
	if (renderBackend != nullptr)
	{
		SyncObject* syncObject = renderBackend->getSyncObject();
		if (syncObject != nullptr)
		{
			syncObject->wait();
		}

		if (view.depthStencilView != nullptr)
		{
			renderBackend->destroyDepthStencilView(view.depthStencilView);
		}
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

		const bool validDepthResources = view.depthTextureObject != nullptr && view.depthStencilView != nullptr;
		assert(validDepthResources && "[RenderWorld][Assert] reason=depth_resource_create_failed");

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
		renderBackendReference.finalizeQueuedSubmissions();
		renderBackendReference.releaseQueuedRenderResources();
		if (!swapChain->isRenderable())
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
			renderBackendReference.finalizeQueuedSubmissions();
			renderBackendReference.releaseQueuedRenderResources();
			return;
		}

		TextureResourceObject* outputResource = swapChain->getCurrentBackBufferResource();
		if (outputResource == nullptr)
		{
			renderBackendReference.finalizeQueuedSubmissions();
			renderBackendReference.releaseQueuedRenderResources();
			return;
		}

		CommandList* commandList = renderBackendReference.acquireCommandList();
		if (commandList == nullptr)
		{
			renderBackendReference.finalizeQueuedSubmissions();
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
