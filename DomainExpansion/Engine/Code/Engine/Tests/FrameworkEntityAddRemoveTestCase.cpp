#include "Engine/Tests/FrameworkEntityAddRemoveTestCase.h"
#include "Engine/Tests/EmptyComponent.h"

const char* FrameworkEntityAddRemoveTestCase::getTestCaseName() const
{
	return "FrameworkEntityAddRemoveTestCase";
}

bool FrameworkEntityAddRemoveTestCase::beginTest(Framework& framework)
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

bool FrameworkEntityAddRemoveTestCase::endTest(Framework& framework)
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

