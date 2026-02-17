#include "Engine/Framework/Component.h"
#include "Engine/Framework/Framework.h"

class EmptyComponent : public Component
{
};

class UpdateOrderRecordingComponent : public Component
{
public:
	UpdateOrderRecordingComponent(
		vector<uint32>* updateOrderStorage,
		uint32* tickCounter,
		const uint32 updateOrderValue)
		: updateOrderStorage(updateOrderStorage)
		, tickCounter(tickCounter)
		, updateOrderValue(updateOrderValue)
	{
	}

	void tick(const float deltaTimeSeconds) override
	{
		unused(deltaTimeSeconds);

		if (tickCounter != nullptr)
		{
			++(*tickCounter);
		}

		if (updateOrderStorage != nullptr)
		{
			updateOrderStorage->push_back(updateOrderValue);
		}
	}

private:
	vector<uint32>* updateOrderStorage = nullptr;
	uint32* tickCounter = nullptr;
	uint32 updateOrderValue = 0;
};

class FrameworkEntityAddRemoveTestCase : public FrameworkTestCase
{
public:
	const char* getTestCaseName() const override
	{
		return "FrameworkEntityAddRemoveTestCase";
	}

	bool beginTest(Framework& framework) override
	{
		unused(framework);

		testWorld.clear();
		testWorld.setWorldName(L"FrameworkEntityAddRemove");

		rootEntityIndex = testWorld.createPlaceableEntity();
		firstChildEntityIndex = testWorld.createEntity();
		secondChildEntityIndex = testWorld.createEntity();

		bool beginResult = true;
		beginResult = expectCondition(
			testWorld.getEntityCount() == 3,
			"begin: create root and two children") && beginResult;
		beginResult = expectCondition(
			rootEntityIndex != invalidEntityIndex && firstChildEntityIndex != invalidEntityIndex && secondChildEntityIndex != invalidEntityIndex,
			"begin: entity indices must be valid") && beginResult;

		return beginResult;
	}

	bool runTest(Framework& framework) override
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

		runResult = expectCondition(
			testWorld.removeEntity(rootEntityIndex),
			"run: remove root entity") && runResult;
		runResult = expectCondition(
			testWorld.getEntityCount() == 2,
			"run: entity storage remains compact after remove") && runResult;

		bool detachedChildrenStateIsValid = true;
		for (uint32 entityIndex = 0; entityIndex < testWorld.getEntityCount(); ++entityIndex)
		{
			const Entity* entity = testWorld.getEntityByIndex(entityIndex);
			if (entity == nullptr || entity->parentEntityIndex != invalidEntityIndex)
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

	bool endTest(Framework& framework) override
	{
		unused(framework);

		testWorld.clear();
		rootEntityIndex = invalidEntityIndex;
		firstChildEntityIndex = invalidEntityIndex;
		secondChildEntityIndex = invalidEntityIndex;

		return expectCondition(
			testWorld.getEntityCount() == 0 && testWorld.getComponentCount() == 0,
			"end: world cleared after add/remove test");
	}

private:
	World testWorld;
	uint32 rootEntityIndex = invalidEntityIndex;
	uint32 firstChildEntityIndex = invalidEntityIndex;
	uint32 secondChildEntityIndex = invalidEntityIndex;
};

class FrameworkEntityUpdateTestCase : public FrameworkTestCase
{
public:
	const char* getTestCaseName() const override
	{
		return "FrameworkEntityUpdateTestCase";
	}

	bool beginTest(Framework& framework) override
	{
		unused(framework);

		testWorld.clear();
		testWorld.setWorldName(L"FrameworkEntityUpdate");

		updateOrderStorage.clear();
		parentTickCount = 0;
		childTickCount = 0;
		parentComponentIndex = invalidComponentIndex;
		childComponentIndex = invalidComponentIndex;

		parentEntityIndex = testWorld.createEntity();
		childEntityIndex = testWorld.createEntity();

		bool beginResult = true;
		beginResult = expectCondition(
			testWorld.addChildEntity(parentEntityIndex, childEntityIndex),
			"begin: attach child to parent for update order test") && beginResult;

		Entity* parentEntity = testWorld.getEntityByIndex(parentEntityIndex);
		Entity* childEntity = testWorld.getEntityByIndex(childEntityIndex);
		beginResult = expectCondition(
			parentEntity != nullptr && childEntity != nullptr,
			"begin: parent and child entity lookup succeeds") && beginResult;

		if (parentEntity == nullptr || childEntity == nullptr)
		{
			return false;
		}

		unique_pointer<UpdateOrderRecordingComponent> parentComponent(
			new UpdateOrderRecordingComponent(&updateOrderStorage, &parentTickCount, 10));
		beginResult = expectCondition(
			parentEntity->addComponent(moveValue(parentComponent)),
			"begin: parent add recording component") && beginResult;

		unique_pointer<UpdateOrderRecordingComponent> childComponent(
			new UpdateOrderRecordingComponent(&updateOrderStorage, &childTickCount, 20));
		beginResult = expectCondition(
			childEntity->addComponent(moveValue(childComponent)),
			"begin: child add recording component") && beginResult;

		parentComponentIndex = parentEntity->getComponentIndex(0);
		childComponentIndex = childEntity->getComponentIndex(0);

		return beginResult;
	}

	bool runTest(Framework& framework) override
	{
		unused(framework);

		bool runResult = true;
		runResult = expectCondition(
			testWorld.tick(0.016f),
			"run: world tick succeeds") && runResult;

		bool updateOrderIsValid = false;
		if (updateOrderStorage.size() == 2)
		{
			updateOrderIsValid = updateOrderStorage[0] == 10 && updateOrderStorage[1] == 20;
		}

		runResult = expectCondition(
			updateOrderIsValid,
			"run: update order is parent then child") && runResult;
		runResult = expectCondition(
			parentTickCount == 1 && childTickCount == 1,
			"run: both parent and child components tick once") && runResult;

		Entity* childEntity = testWorld.getEntityByIndex(childEntityIndex);
		runResult = expectCondition(
			childEntity != nullptr,
			"run: child entity lookup before remove") && runResult;

		if (childEntity != nullptr)
		{
			runResult = expectCondition(
				childEntity->removeComponent(childComponentIndex),
				"run: child remove owned component") && runResult;
		}

		updateOrderStorage.clear();
		runResult = expectCondition(
			testWorld.tick(0.016f),
			"run: world tick succeeds after child component remove") && runResult;

		bool secondUpdateOrderIsValid = false;
		if (updateOrderStorage.size() == 1)
		{
			secondUpdateOrderIsValid = updateOrderStorage[0] == 10;
		}

		runResult = expectCondition(
			secondUpdateOrderIsValid,
			"run: only parent component ticks after child remove") && runResult;
		runResult = expectCondition(
			testWorld.getComponentCount() == 1,
			"run: component storage compacted to one") && runResult;

		return runResult;
	}

	bool endTest(Framework& framework) override
	{
		unused(framework);

		testWorld.clear();
		parentEntityIndex = invalidEntityIndex;
		childEntityIndex = invalidEntityIndex;
		parentComponentIndex = invalidComponentIndex;
		childComponentIndex = invalidComponentIndex;
		updateOrderStorage.clear();
		parentTickCount = 0;
		childTickCount = 0;

		return expectCondition(
			testWorld.getEntityCount() == 0 && testWorld.getComponentCount() == 0,
			"end: world cleared after update test");
	}

private:
	World testWorld;
	uint32 parentEntityIndex = invalidEntityIndex;
	uint32 childEntityIndex = invalidEntityIndex;
	uint32 childComponentIndex = invalidComponentIndex;
	uint32 parentComponentIndex = invalidComponentIndex;
	vector<uint32> updateOrderStorage;
	uint32 parentTickCount = 0;
	uint32 childTickCount = 0;
};

void Framework::registerTest()
{
	clearTestCases();
	addTestCase(unique_pointer<FrameworkTestCase>(new FrameworkEntityAddRemoveTestCase()));
	addTestCase(unique_pointer<FrameworkTestCase>(new FrameworkEntityUpdateTestCase()));
}
