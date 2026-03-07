#include "Engine/Tests/FrameworkEntityUpdateTestCase.h"
#include "Engine/Tests/UpdateOrderRecordingComponent.h"

class InitTrackingUpdateOrderComponent final : public Component
{
public:
	InitTrackingUpdateOrderComponent(
		vector<uint32>* updateOrderStorage,
		uint32* tickCounter,
		uint32 updateOrderValue,
		uint32* initCounter,
		vector<uint32>* initEntityIndexStorage,
		vector<uint32>* initComponentIndexStorage)
		: updateOrderStorage(updateOrderStorage)
		, tickCounter(tickCounter)
		, updateOrderValue(updateOrderValue)
		, initCounter(initCounter)
		, initEntityIndexStorage(initEntityIndexStorage)
		, initComponentIndexStorage(initComponentIndexStorage)
	{
	}

	void tick(float deltaTimeSeconds) override
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

protected:
	void initComponent() override
	{
		if (initCounter != nullptr)
		{
			++(*initCounter);
		}

		if (initEntityIndexStorage != nullptr)
		{
			initEntityIndexStorage->push_back(getOwnerEntityIndex());
		}

		if (initComponentIndexStorage != nullptr)
		{
			initComponentIndexStorage->push_back(getComponentIndex());
		}
	}

private:
	vector<uint32>* updateOrderStorage = nullptr;
	uint32* tickCounter = nullptr;
	uint32 updateOrderValue = 0;
	uint32* initCounter = nullptr;
	vector<uint32>* initEntityIndexStorage = nullptr;
	vector<uint32>* initComponentIndexStorage = nullptr;
};

const char* FrameworkEntityUpdateTestCase::getTestCaseName() const
{
	return "FrameworkEntityUpdateTestCase";
}

bool FrameworkEntityUpdateTestCase::beginTest(Framework& framework)
{
	unused(framework);

	testWorld.clear();
	testWorld.setWorldName(L"FrameworkEntityUpdate");

	updateOrderStorage.clear();
	childInitEntityIndexStorage.clear();
	childInitComponentIndexStorage.clear();
	parentTickCount = 0;
	childTickCount = 0;
	childInitCount = 0;
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

	unique_pointer<InitTrackingUpdateOrderComponent> childComponent(
		new InitTrackingUpdateOrderComponent(
			&updateOrderStorage,
			&childTickCount,
			20,
			&childInitCount,
			&childInitEntityIndexStorage,
			&childInitComponentIndexStorage));
	beginResult = expectCondition(
		childEntity->addComponent(moveValue(childComponent)),
		"begin: child add recording component") && beginResult;

	parentComponentIndex = parentEntity->getComponentIndex(0);
	childComponentIndex = childEntity->getComponentIndex(0);
	beginResult = expectCondition(
		childInitCount == 1
			&& childInitEntityIndexStorage.size() == 1
			&& childInitEntityIndexStorage[0] == childEntityIndex
			&& childInitComponentIndexStorage.size() == 1
			&& childInitComponentIndexStorage[0] == childComponentIndex,
		"begin: child component initComponent called on add") && beginResult;

	return beginResult;
}

bool FrameworkEntityUpdateTestCase::runTest(Framework& framework)
{
	unused(framework);

	bool runResult = true;
	testWorld.tick(0.016f);
	runResult = expectCondition(
		true,
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

	Entity* parentEntity = testWorld.getEntityByIndex(parentEntityIndex);
	runResult = expectCondition(
		parentEntity != nullptr,
		"run: parent entity lookup before remove") && runResult;

	if (parentEntity != nullptr)
	{
		runResult = expectCondition(
			parentEntity->removeComponent(parentComponentIndex),
			"run: parent remove owned component") && runResult;
	}

	runResult = expectCondition(
		childInitCount == 2
			&& childInitEntityIndexStorage.size() == 2
			&& childInitEntityIndexStorage[1] == childEntityIndex
			&& childInitComponentIndexStorage.size() == 2
			&& childInitComponentIndexStorage[1] == 0,
		"run: child component initComponent recalled after component compaction") && runResult;

	updateOrderStorage.clear();
	testWorld.tick(0.016f);
	runResult = expectCondition(
		true,
		"run: world tick succeeds after child component remove") && runResult;

	bool secondUpdateOrderIsValid = false;
	if (updateOrderStorage.size() == 1)
	{
		secondUpdateOrderIsValid = updateOrderStorage[0] == 20;
	}

	runResult = expectCondition(
		secondUpdateOrderIsValid,
		"run: only child component ticks after parent remove") && runResult;
	runResult = expectCondition(
		testWorld.getComponentCount() == 1,
		"run: component storage compacted to one") && runResult;

	return runResult;
}

bool FrameworkEntityUpdateTestCase::endTest(Framework& framework)
{
	unused(framework);

	testWorld.clear();
	parentEntityIndex = invalidEntityIndex;
	childEntityIndex = invalidEntityIndex;
	parentComponentIndex = invalidComponentIndex;
	childComponentIndex = invalidComponentIndex;
	updateOrderStorage.clear();
	childInitEntityIndexStorage.clear();
	childInitComponentIndexStorage.clear();
	parentTickCount = 0;
	childTickCount = 0;
	childInitCount = 0;

	return expectCondition(
		testWorld.getEntityCount() == 0 && testWorld.getComponentCount() == 0,
		"end: world cleared after update test");
}
