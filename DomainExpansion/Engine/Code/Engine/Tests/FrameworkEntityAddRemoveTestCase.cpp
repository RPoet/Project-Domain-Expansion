#include "Engine/Tests/FrameworkEntityAddRemoveTestCase.h"

#include "Bridge/EntityBridge.h"
#include "Engine/Tests/EmptyComponent.h"

class InitTrackingEmptyComponent final : public Component
{
public:
	InitTrackingEmptyComponent(
		uint32* initCounter,
		vector<uint32>* ownerEntityIndexStorage,
		vector<uint32>* componentIndexStorage)
		: initCounter(initCounter)
		, ownerEntityIndexStorage(ownerEntityIndexStorage)
		, componentIndexStorage(componentIndexStorage)
	{
	}

protected:
	void initComponent() override
	{
		if (initCounter != nullptr)
		{
			++(*initCounter);
		}

		if (ownerEntityIndexStorage != nullptr)
		{
			ownerEntityIndexStorage->push_back(getOwnerEntityIndex());
		}

		if (componentIndexStorage != nullptr)
		{
			componentIndexStorage->push_back(getComponentIndex());
		}
	}

private:
	uint32* initCounter = nullptr;
	vector<uint32>* ownerEntityIndexStorage = nullptr;
	vector<uint32>* componentIndexStorage = nullptr;
};

const char* FrameworkEntityAddRemoveTestCase::getTestCaseName() const
{
	return "FrameworkEntityAddRemoveTestCase";
}

bool FrameworkEntityAddRemoveTestCase::beginTest(Framework& framework)
{
	unused(framework);

	testWorld.clear();
	testWorld.setWorldName(L"FrameworkEntityAddRemove");
	movedComponentOwnerEntityIndexStorage.clear();
	movedComponentIndexStorage.clear();
	movedComponentInitCount = 0;

	rootEntityIndex = testWorld.createPlaceableEntity();
	firstChildEntityIndex = testWorld.createEntity();
	secondChildEntityIndex = testWorld.createEntity();
	const Entity* rootEntity = testWorld.getEntityByIndex(rootEntityIndex);
	const Entity* firstChildEntity = testWorld.getEntityByIndex(firstChildEntityIndex);
	const Entity* secondChildEntity = testWorld.getEntityByIndex(secondChildEntityIndex);
	rootEntityBridgeHandle = rootEntity != nullptr ? rootEntity->getEntityHandle() : uint32MaxValue;
	firstChildEntityBridgeHandle = firstChildEntity != nullptr ? firstChildEntity->getEntityHandle() : uint32MaxValue;
	secondChildEntityBridgeHandle = secondChildEntity != nullptr ? secondChildEntity->getEntityHandle() : uint32MaxValue;

	bool beginResult = true;
	beginResult = expectCondition(
		testWorld.getEntityCount() == 3,
		"begin: create root and two children") && beginResult;
	beginResult = expectCondition(
		rootEntityIndex != invalidEntityIndex && firstChildEntityIndex != invalidEntityIndex && secondChildEntityIndex != invalidEntityIndex,
		"begin: entity indices must be valid") && beginResult;
	beginResult = expectCondition(
		rootEntityBridgeHandle != EntityBridge::invalidPackedHandle
			&& firstChildEntityBridgeHandle != EntityBridge::invalidPackedHandle
			&& secondChildEntityBridgeHandle != EntityBridge::invalidPackedHandle,
		"begin: entity bridge handles created") && beginResult;
	beginResult = expectCondition(
		EntityBridge::get().isHandleAlive(rootEntityBridgeHandle)
			&& EntityBridge::get().isHandleAlive(firstChildEntityBridgeHandle)
			&& EntityBridge::get().isHandleAlive(secondChildEntityBridgeHandle),
		"begin: entity bridge handles alive after create") && beginResult;

	return beginResult;
}

bool FrameworkEntityAddRemoveTestCase::runTest(Framework& framework)
{
	unused(framework);

	bool runResult = true;
	runResult = expectCondition(
		testWorld.addChildEntity(rootEntityIndex, firstChildEntityIndex),
		"run: attach first child to root") && runResult;
	runResult = expectCondition(
		testWorld.addChildEntity(rootEntityIndex, secondChildEntityIndex),
		"run: attach second child to root") && runResult;
	runResult = expectCondition(
		!testWorld.addChildEntity(rootEntityIndex, rootEntityIndex),
		"run: reject self-parenting") && runResult;
	runResult = expectCondition(
		!testWorld.addChildEntity(firstChildEntityIndex, rootEntityIndex),
		"run: reject cycle parenting") && runResult;

	Entity* rootEntity = testWorld.getEntityByIndex(rootEntityIndex);
	Entity* firstChildEntity = testWorld.getEntityByIndex(firstChildEntityIndex);
	Entity* secondChildEntity = testWorld.getEntityByIndex(secondChildEntityIndex);
	runResult = expectCondition(
		rootEntity != nullptr && firstChildEntity != nullptr && secondChildEntity != nullptr,
		"run: entity lookup must succeed before component checks") && runResult;

	if (rootEntity != nullptr && firstChildEntity != nullptr)
	{
		unique_pointer<EmptyComponent> rootComponent(new EmptyComponent());
		runResult = expectCondition(
			rootEntity->addComponent(moveValue(rootComponent)),
			"run: root entity add component via world delegation") && runResult;

		unique_pointer<EmptyComponent> childComponent(new EmptyComponent());
		runResult = expectCondition(
			firstChildEntity->addComponent(moveValue(childComponent)),
			"run: child entity add component via world delegation") && runResult;

		const uint32 rootComponentIndex = rootEntity->getComponentIndex(0);
		runResult = expectCondition(
			!firstChildEntity->removeComponent(rootComponentIndex),
			"run: non-owner removeComponent must fail") && runResult;
	}

	if (secondChildEntity != nullptr)
	{
		unique_pointer<InitTrackingEmptyComponent> movedComponent(
			new InitTrackingEmptyComponent(
				&movedComponentInitCount,
				&movedComponentOwnerEntityIndexStorage,
				&movedComponentIndexStorage));
		runResult = expectCondition(
			secondChildEntity->addComponent(moveValue(movedComponent)),
			"run: second child add init tracking component") && runResult;
		runResult = expectCondition(
			movedComponentInitCount == 1
				&& movedComponentOwnerEntityIndexStorage.size() == 1
				&& movedComponentOwnerEntityIndexStorage[0] == secondChildEntityIndex
				&& movedComponentIndexStorage.size() == 1
				&& movedComponentIndexStorage[0] == 2,
			"run: second child component initComponent called on add") && runResult;
	}

	runResult = expectCondition(
		testWorld.removeEntity(rootEntityIndex),
		"run: remove root entity") && runResult;
	runResult = expectCondition(
		testWorld.getEntityCount() == 2,
		"run: entity storage remains compact after remove") && runResult;
	runResult = expectCondition(
		EntityBridge::get().isHandleAlive(rootEntityBridgeHandle),
		"run: removed entity bridge handle delete delayed before processFrame") && runResult;

	const Entity* movedEntity = testWorld.getEntityByIndex(0);
	const Entity* remainingEntity = testWorld.getEntityByIndex(1);
	const uint32 movedEntityBridgeHandle = movedEntity != nullptr ? movedEntity->getEntityHandle() : uint32MaxValue;
	const EntityBridge::DynamicData* movedDynamicData = EntityBridge::get().getDynamicData(movedEntityBridgeHandle);
	runResult = expectCondition(
			movedEntityBridgeHandle == secondChildEntityBridgeHandle
				&& remainingEntity != nullptr
				&& remainingEntity->getEntityHandle() == firstChildEntityBridgeHandle
				&& movedDynamicData != nullptr
				&& movedDynamicData->entityIndex == secondChildEntityIndex,
		"run: moved entity keeps bridge handle and defers entity index update") && runResult;
	runResult = expectCondition(
		movedComponentInitCount == 3
			&& movedComponentOwnerEntityIndexStorage.size() == 3
			&& movedComponentOwnerEntityIndexStorage[1] == secondChildEntityIndex
			&& movedComponentOwnerEntityIndexStorage[2] == 0
			&& movedComponentIndexStorage.size() == 3
			&& movedComponentIndexStorage[1] == 0
			&& movedComponentIndexStorage[2] == 0,
		"run: moved entity component initComponent recalled after component and entity compaction") && runResult;

	EntityBridge::get().processFrame();
	movedDynamicData = EntityBridge::get().getDynamicData(movedEntityBridgeHandle);
	runResult = expectCondition(
		!EntityBridge::get().isHandleAlive(rootEntityBridgeHandle),
		"run: removed entity bridge handle deleted on processFrame") && runResult;
	runResult = expectCondition(
		movedDynamicData != nullptr && movedDynamicData->entityIndex == 0,
		"run: moved entity bridge entity index updated on processFrame") && runResult;

	bool detachedChildrenStateIsValid = true;
	for (uint32 entityIndex = 0; entityIndex < testWorld.getEntityCount(); ++entityIndex)
	{
		const Entity* entity = testWorld.getEntityByIndex(entityIndex);
		if (entity == nullptr || entity->getParentEntityIndex() != invalidEntityIndex)
		{
			detachedChildrenStateIsValid = false;
			break;
		}
	}

	runResult = expectCondition(
		detachedChildrenStateIsValid,
		"run: removed root children become detached roots") && runResult;

	return runResult;
}

bool FrameworkEntityAddRemoveTestCase::endTest(Framework& framework)
{
	unused(framework);

	const EntityBridge::PackedHandle remainingFirstChildHandle = firstChildEntityBridgeHandle;
	const EntityBridge::PackedHandle remainingSecondChildHandle = secondChildEntityBridgeHandle;
	testWorld.clear();
	EntityBridge::get().processFrame();
	rootEntityIndex = invalidEntityIndex;
	firstChildEntityIndex = invalidEntityIndex;
	secondChildEntityIndex = invalidEntityIndex;
	rootEntityBridgeHandle = EntityBridge::invalidPackedHandle;
	firstChildEntityBridgeHandle = EntityBridge::invalidPackedHandle;
	secondChildEntityBridgeHandle = EntityBridge::invalidPackedHandle;
	movedComponentOwnerEntityIndexStorage.clear();
	movedComponentIndexStorage.clear();
	movedComponentInitCount = 0;

	return expectCondition(
		testWorld.getEntityCount() == 0
			&& testWorld.getComponentCount() == 0
			&& !EntityBridge::get().isHandleAlive(remainingFirstChildHandle)
			&& !EntityBridge::get().isHandleAlive(remainingSecondChildHandle),
		"end: world cleared after add/remove test");
}
