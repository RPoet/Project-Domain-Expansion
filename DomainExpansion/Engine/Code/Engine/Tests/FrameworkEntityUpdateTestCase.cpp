#include "Engine/Tests/FrameworkEntityUpdateTestCase.h"
#include "Engine/Tests/UpdateOrderRecordingComponent.h"

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
	testWorld.tick(0.016f);
	runResult = expectCondition(
		true,
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

bool FrameworkEntityUpdateTestCase::endTest(Framework& framework)
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
