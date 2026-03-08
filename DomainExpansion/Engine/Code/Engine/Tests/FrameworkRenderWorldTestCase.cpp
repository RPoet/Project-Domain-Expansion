#include "Engine/Tests/FrameworkRenderWorldTestCase.h"

#include "Engine/Framework/Framework.h"
#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Module/Asset/MeshStreaming.h"
#include "Render/RenderWorld.h"

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
	RenderWorld renderWorld = {};
	RenderWorld::BuildResult buildResult = renderWorld.build();

	bool runResult = true;
	runResult = expectCondition(buildResult.meshDrawData.size() == 1, "run: render world builds one visible mesh draw") && runResult;

	const RenderWorld::MeshDrawData* meshDrawData =
		buildResult.meshDrawData.empty() ? nullptr : &buildResult.meshDrawData[0];
	runResult = expectCondition(
		meshDrawData != nullptr
			&& meshDrawData->meshAssetHandle != nullptr
			&& meshDrawData->meshAssetHandle->meshRelativePath == "Meshes/Sphere.obj"
			&& meshDrawData->transform.positionX == 3.0f
			&& meshDrawData->transform.positionY == 4.0f
			&& meshDrawData->transform.scaleZ == 5.0f,
		"run: render world joins mesh bridge and entity bridge data") && runResult;

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
		if (component == nullptr || component->getComponentType() != ComponentType::meshComponent)
		{
			continue;
		}

		meshComponent = static_cast<MeshComponent*>(component);
		break;
	}

	runResult = expectCondition(meshComponent != nullptr, "run: mesh component exists before mutation") && runResult;
	if (meshComponent != nullptr)
	{
		meshComponent->visible = false;
	}

	placeableEntity->transform.positionX = 30.0f;
	activeWorld->tick(0.016f);
	return runResult;
}

bool FrameworkRenderWorldTestCase::endTest(Framework& framework)
{
	RenderWorld renderWorld = {};
	RenderWorld::BuildResult buildResult = renderWorld.build();

	bool endResult = true;
	endResult = expectCondition(buildResult.meshDrawData.empty(), "end: invisible mesh is removed from render world") && endResult;

	framework.unloadWorld(worldIndex);
	worldIndex = invalidWorldIndex;
	entityIndex = invalidEntityIndex;
	return endResult;
}
