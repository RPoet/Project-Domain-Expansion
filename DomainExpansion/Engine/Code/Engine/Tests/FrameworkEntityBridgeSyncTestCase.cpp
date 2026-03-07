#include "Engine/Tests/FrameworkEntityBridgeSyncTestCase.h"

#include "Bridge/EntityBridge.h"
#include "Bridge/MeshBridge.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"

const char* FrameworkEntityBridgeSyncTestCase::getTestCaseName() const
{
	return "FrameworkEntityBridgeSyncTestCase";
}

bool FrameworkEntityBridgeSyncTestCase::beginTest(Framework& framework)
{
	worldIndex = framework.createWorld(L"FrameworkEntityBridgeSync");
	bool beginResult = true;
	beginResult = expectCondition(framework.loadWorld(worldIndex), "begin: load entity bridge sync world") && beginResult;

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

	placeableEntity->transform.positionX = 1.0f;
	placeableEntity->transform.positionY = 2.0f;
	placeableEntity->transform.scaleZ = 3.0f;

	unique_pointer<MeshComponent> meshComponent(new MeshComponent());
	meshComponent->meshRelativePath = "Meshes/Sphere.obj";
	meshComponent->lodLevel = 2;
	meshComponent->visible = true;
	beginResult = expectCondition(
		placeableEntity->addComponent(moveValue(meshComponent)),
		"begin: add mesh component to placeable entity") && beginResult;

	entityHandle = placeableEntity->getEntityHandle();
	beginResult = expectCondition(entityHandle != invalidBridgeHandle, "begin: entity bridge handle exists") && beginResult;
	MeshComponent* createdMeshComponent = nullptr;
	for (uint32 componentArrayIndex = 0; componentArrayIndex < placeableEntity->getComponentCount(); ++componentArrayIndex)
	{
		Component* component = activeWorld->getComponentByIndex(placeableEntity->getComponentIndex(componentArrayIndex));
		if (component == nullptr || component->getComponentType() != ComponentType::meshComponent)
		{
			continue;
		}

		createdMeshComponent = static_cast<MeshComponent*>(component);
		break;
	}

	meshHandle = createdMeshComponent != nullptr
		? createdMeshComponent->getMeshHandle()
		: invalidBridgeHandle;
	beginResult = expectCondition(meshHandle != invalidBridgeHandle, "begin: mesh bridge handle exists") && beginResult;
	activeWorld->tick(0.016f);
	return beginResult;
}

bool FrameworkEntityBridgeSyncTestCase::runTest(Framework& framework)
{
	unused(framework);

	const EntityBridge::DynamicData* entityDynamicData = EntityBridge::get().getDynamicData(entityHandle);
	bool runResult = true;
	runResult = expectCondition(
		entityDynamicData != nullptr
			&& entityDynamicData->entityIndex == entityIndex
			&& entityDynamicData->hasTransform
			&& entityDynamicData->transform.positionX == 1.0f
			&& entityDynamicData->transform.positionY == 2.0f
			&& entityDynamicData->transform.scaleZ == 3.0f,
		"run: entity tick synced transform") && runResult;
	if (!runResult || entityDynamicData == nullptr)
	{
		return false;
	}

	const MeshBridge::StaticData* meshStaticData = MeshBridge::get().getStaticData(meshHandle);
	const MeshBridge::DynamicData* meshDynamicData = MeshBridge::get().getDynamicData(meshHandle);
	runResult = expectCondition(
		meshStaticData != nullptr
			&& meshStaticData->entityHandle == entityHandle
			&& meshStaticData->meshRelativePath == "Meshes/Sphere.obj"
			&& meshStaticData->lodLevel == 2
			&& meshStaticData->meshAssetHandle != nullptr
			&& meshDynamicData != nullptr
			&& meshDynamicData->visible,
		"run: mesh bridge synced owner entity link, static asset identity, and dynamic visibility") && runResult;
	if (!runResult)
	{
		return false;
	}

	World* activeWorld = framework.getActiveWorld();
	PlaceableEntity* placeableEntity = activeWorld != nullptr
		? static_cast<PlaceableEntity*>(activeWorld->getEntityByIndex(entityIndex))
		: nullptr;
	runResult = expectCondition(placeableEntity != nullptr, "run: placeable entity exists before mutation") && runResult;
	if (placeableEntity == nullptr)
	{
		return false;
	}

	placeableEntity->transform.positionX = 11.0f;
	placeableEntity->transform.scaleZ = 13.0f;
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

	activeWorld->tick(0.016f);

	return runResult;
}

bool FrameworkEntityBridgeSyncTestCase::endTest(Framework& framework)
{
	bool endResult = true;
	const EntityBridge::DynamicData* entityDynamicData = EntityBridge::get().getDynamicData(entityHandle);
	endResult = expectCondition(
		entityDynamicData != nullptr
			&& entityDynamicData->transform.positionX == 11.0f
			&& entityDynamicData->transform.scaleZ == 13.0f,
		"end: entity tick applied transform mutation on postUpdate") && endResult;

	if (entityDynamicData != nullptr)
	{
		const MeshBridge::DynamicData* meshDynamicData = MeshBridge::get().getDynamicData(meshHandle);
		endResult = expectCondition(
			meshDynamicData != nullptr && !meshDynamicData->visible,
			"end: mesh bridge applied visibility mutation on postUpdate") && endResult;
	}

	framework.unloadWorld(worldIndex);
	worldIndex = invalidWorldIndex;
	entityIndex = invalidEntityIndex;
	entityHandle = invalidBridgeHandle;
	meshHandle = invalidBridgeHandle;
	return endResult;
}
