#include "EngineTests/Tests/FrameworkRenderWorldTestCase.h"

#include "Bridge/CameraBridge.h"
#include "Engine/Framework/CameraComponent.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Module/MeshStreaming/MeshStreaming.h"
#include "Bridge/MeshBridge.h"
#include "Render/RenderWorld.h"

class TestBufferResourceObject final : public BufferResourceObject
{
};

static bool containsCameraHandle(const vector<BridgeHandle>& cameraHandles, const BridgeHandle cameraHandle)
{
	for (uint32 cameraIndex = 0; cameraIndex < static_cast<uint32>(cameraHandles.size()); ++cameraIndex)
	{
		if (cameraHandles[cameraIndex] == cameraHandle)
		{
			return true;
		}
	}

	return false;
}

static CameraComponent* getEditorCameraComponent(World* world, uint32& outEntityIndex, PlaceableEntity*& outEntity)
{
	outEntityIndex = invalidEntityIndex;
	outEntity = nullptr;
	if (world == nullptr)
	{
		return nullptr;
	}

	for (uint32 entityIndex = 0; entityIndex < world->getEntityCount(); ++entityIndex)
	{
		Entity* entity = world->getEntityByIndex(entityIndex);
		if (entity == nullptr)
		{
			continue;
		}

		for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
		{
			Component* component = world->getComponentByIndex(entity->getComponentIndex(componentArrayIndex));
			if (component == nullptr || component->getComponentType() != CameraComponent::staticComponentType)
			{
				continue;
			}

			CameraComponent* cameraComponent = static_cast<CameraComponent*>(component);
			if (!cameraComponent->editorCamera)
			{
				continue;
			}

			outEntityIndex = entityIndex;
			outEntity = dynamic_cast<PlaceableEntity*>(entity);
			return cameraComponent;
		}
	}

	return nullptr;
}

const char* FrameworkRenderWorldTestCase::getTestCaseName() const
{
	return "FrameworkRenderWorldTestCase";
}

bool FrameworkRenderWorldTestCase::beginTest(Framework& framework)
{
	worldIndex = framework.createWorld(L"FrameworkRenderWorld");
	bool beginResult = true;
	beginResult = expectCondition(framework.loadWorld(worldIndex), "begin: load render world test world") && beginResult;

	World* activeWorld = framework.getActiveWorld();
	beginResult = expectCondition(activeWorld != nullptr, "begin: active world exists") && beginResult;
	if (activeWorld == nullptr)
	{
		return false;
	}

	PlaceableEntity* editorCameraEntity = nullptr;
	CameraComponent* editorCameraComponent = getEditorCameraComponent(activeWorld, editorCameraEntityIndex, editorCameraEntity);
	beginResult = expectCondition(
		editorCameraComponent != nullptr
			&& editorCameraEntity != nullptr
			&& editorCameraEntityIndex != invalidEntityIndex,
		"begin: editor camera exists on loaded world") && beginResult;

	entityIndex = activeWorld->createPlaceableEntity();
	PlaceableEntity* placeableEntity = static_cast<PlaceableEntity*>(activeWorld->getEntityByIndex(entityIndex));
	beginResult = expectCondition(placeableEntity != nullptr, "begin: placeable entity exists") && beginResult;
	if (placeableEntity == nullptr)
	{
		return false;
	}

	placeableEntity->transform.positionX = 3.0f;
	placeableEntity->transform.positionY = 4.0f;
	placeableEntity->transform.scaleZ = 5.0f;

	unique_pointer<MeshComponent> meshComponent(new MeshComponent());
	meshComponent->meshRelativePath = "Meshes/Sphere.obj";
	meshComponent->visible = true;
	beginResult = expectCondition(
		placeableEntity->addComponent(moveValue(meshComponent)),
		"begin: add mesh component") && beginResult;

	activeWorld->tick(0.016f);
	return beginResult;
}

bool FrameworkRenderWorldTestCase::runTest(Framework& framework)
{
	bool runResult = true;
	World* activeWorld = framework.getActiveWorld();
	PlaceableEntity* placeableEntity = activeWorld != nullptr
		? static_cast<PlaceableEntity*>(activeWorld->getEntityByIndex(entityIndex))
		: nullptr;
	runResult = expectCondition(placeableEntity != nullptr, "run: placeable entity exists before mutation") && runResult;
	if (placeableEntity == nullptr)
	{
		return false;
	}

	MeshComponent* meshComponent = nullptr;
	for (uint32 componentArrayIndex = 0; componentArrayIndex < placeableEntity->getComponentCount(); ++componentArrayIndex)
	{
		Component* component = activeWorld->getComponentByIndex(placeableEntity->getComponentIndex(componentArrayIndex));
		if (component == nullptr || component->getComponentType() != MeshComponent::staticComponentType)
		{
			continue;
		}

		meshComponent = static_cast<MeshComponent*>(component);
		break;
	}

	runResult = expectCondition(meshComponent != nullptr, "run: mesh component exists before mutation") && runResult;
	if (meshComponent == nullptr)
	{
		return false;
	}

	const MeshBridge::StaticData* meshStaticData = MeshBridge::get().getStaticData(meshComponent->getMeshHandle());
	runResult = expectCondition(meshStaticData != nullptr, "run: mesh bridge static data exists before gpu injection") && runResult;
	if (meshStaticData == nullptr || meshStaticData->meshAssetHandle == nullptr)
	{
		return false;
	}

	shared_pointer<MeshAssetHandle> actualMeshAssetHandle = meshStaticData->meshAssetHandle;
	actualMeshAssetHandle->gpuState = MeshAssetGpuState::ready;
	actualMeshAssetHandle->requiredVertexBufferFlags =
		getMeshBufferSignatureFlag(MeshBufferSignature::position)
		| getMeshBufferSignatureFlag(MeshBufferSignature::normal)
		| getMeshBufferSignatureFlag(MeshBufferSignature::texcoord);
	actualMeshAssetHandle->activeVertexBufferFlags = actualMeshAssetHandle->requiredVertexBufferFlags;
	actualMeshAssetHandle->vertexBufferObjects[getMeshBufferSignatureIndex(MeshBufferSignature::position)] =
		unique_pointer<BufferResourceObject>(new TestBufferResourceObject());
	actualMeshAssetHandle->vertexBufferObjects[getMeshBufferSignatureIndex(MeshBufferSignature::normal)] =
		unique_pointer<BufferResourceObject>(new TestBufferResourceObject());
	actualMeshAssetHandle->vertexBufferObjects[getMeshBufferSignatureIndex(MeshBufferSignature::texcoord)] =
		unique_pointer<BufferResourceObject>(new TestBufferResourceObject());
	actualMeshAssetHandle->vertexBufferSizesInBytes[getMeshBufferSignatureIndex(MeshBufferSignature::position)] =
		static_cast<uint32>(sizeof(MeshAsset::PositionData) * 4);
	actualMeshAssetHandle->vertexBufferSizesInBytes[getMeshBufferSignatureIndex(MeshBufferSignature::normal)] =
		static_cast<uint32>(sizeof(MeshAsset::NormalData) * 4);
	actualMeshAssetHandle->vertexBufferSizesInBytes[getMeshBufferSignatureIndex(MeshBufferSignature::texcoord)] =
		static_cast<uint32>(sizeof(MeshAsset::TexcoordData) * 4);
	actualMeshAssetHandle->vertexBufferStridesInBytes[getMeshBufferSignatureIndex(MeshBufferSignature::position)] =
		static_cast<uint32>(sizeof(MeshAsset::PositionData));
	actualMeshAssetHandle->vertexBufferStridesInBytes[getMeshBufferSignatureIndex(MeshBufferSignature::normal)] =
		static_cast<uint32>(sizeof(MeshAsset::NormalData));
	actualMeshAssetHandle->vertexBufferStridesInBytes[getMeshBufferSignatureIndex(MeshBufferSignature::texcoord)] =
		static_cast<uint32>(sizeof(MeshAsset::TexcoordData));
	actualMeshAssetHandle->indexBufferObject = unique_pointer<BufferResourceObject>(new TestBufferResourceObject());
	actualMeshAssetHandle->indexBufferSizeInBytes = static_cast<uint32>(sizeof(uint32) * 12);

	RenderWorld renderWorld = {};
	RenderWorldBuildResult buildResult = renderWorld.build();
	runResult = expectCondition(buildResult.meshDrawData.size() == 1, "run: render world builds one visible gpu-ready mesh draw") && runResult;
	PlaceableEntity* editorCameraEntity = nullptr;
	uint32 resolvedEditorCameraEntityIndex = invalidEntityIndex;
	CameraComponent* editorCameraComponent = getEditorCameraComponent(activeWorld, resolvedEditorCameraEntityIndex, editorCameraEntity);
	const BridgeHandle editorCameraHandle = editorCameraComponent != nullptr ? editorCameraComponent->getCameraHandle() : invalidBridgeHandle;
	const CameraBridge::DynamicData* cameraDynamicData = CameraBridge::get().getDynamicData(editorCameraHandle);
	const CameraBridge::StaticData* cameraStaticData = CameraBridge::get().getStaticData(editorCameraHandle);
	runResult = expectCondition(
		!buildResult.cameraHandles.empty()
			&& editorCameraHandle != invalidBridgeHandle
			&& containsCameraHandle(buildResult.cameraHandles, editorCameraHandle)
			&& cameraStaticData != nullptr
			&& cameraStaticData->editorCamera
			&& cameraDynamicData != nullptr
			&& cameraDynamicData->viewMatrix.value[14] == 4.0f,
		"run: render world resolves active camera handles including editor camera") && runResult;

	const RenderWorldMeshDrawData* meshDrawData = buildResult.meshDrawData.empty() ? nullptr : &buildResult.meshDrawData[0];
	runResult = expectCondition(
		meshDrawData != nullptr
			&& meshDrawData->meshAssetHandle == actualMeshAssetHandle
			&& meshDrawData->meshAssetHandle->meshRelativePath == "Meshes/Sphere.obj"
			&& meshDrawData->transform.positionX == 3.0f
			&& meshDrawData->transform.positionY == 4.0f
			&& meshDrawData->transform.scaleZ == 5.0f,
		"run: render world joins gpu-ready mesh bridge and entity bridge data") && runResult;

	meshComponent->visible = false;
	placeableEntity->transform.positionX = 30.0f;
	editorCameraEntity = activeWorld != nullptr
		? static_cast<PlaceableEntity*>(activeWorld->getEntityByIndex(editorCameraEntityIndex))
		: nullptr;
	runResult = expectCondition(editorCameraEntity != nullptr, "run: editor camera entity exists before mutation") && runResult;
	if (editorCameraEntity != nullptr)
	{
		editorCameraEntity->transform.positionX = 2.0f;
		editorCameraEntity->transform.rotationYaw = 1.0f;
	}
	activeWorld->tick(0.016f);
	return runResult;
}

bool FrameworkRenderWorldTestCase::endTest(Framework& framework)
{
	RenderWorld renderWorld = {};
	RenderWorldBuildResult buildResult = renderWorld.build();
	World* activeWorld = framework.getActiveWorld();
	PlaceableEntity* editorCameraEntity = nullptr;
	uint32 resolvedEditorCameraEntityIndex = invalidEntityIndex;
	CameraComponent* editorCameraComponent = getEditorCameraComponent(activeWorld, resolvedEditorCameraEntityIndex, editorCameraEntity);
	const BridgeHandle editorCameraHandle = editorCameraComponent != nullptr ? editorCameraComponent->getCameraHandle() : invalidBridgeHandle;
	const CameraBridge::DynamicData* cameraDynamicData = CameraBridge::get().getDynamicData(editorCameraHandle);

	bool endResult = true;
	endResult = expectCondition(buildResult.meshDrawData.empty(), "end: invisible mesh is removed from render world") && endResult;
	endResult = expectCondition(
		editorCameraHandle != invalidBridgeHandle
			&& containsCameraHandle(buildResult.cameraHandles, editorCameraHandle)
			&& cameraDynamicData != nullptr,
		"end: editor camera handle remains available in render world camera list") && endResult;

	framework.unloadWorld(worldIndex);
	worldIndex = invalidWorldIndex;
	entityIndex = invalidEntityIndex;
	editorCameraEntityIndex = invalidEntityIndex;
	return endResult;
}
