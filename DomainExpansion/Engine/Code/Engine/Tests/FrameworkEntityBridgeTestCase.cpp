#include "Engine/Tests/FrameworkEntityBridgeTestCase.h"

#include "Bridge/EntityBridge.h"

const char* FrameworkEntityBridgeTestCase::getTestCaseName() const
{
	return "FrameworkEntityBridgeTestCase";
}

bool FrameworkEntityBridgeTestCase::beginTest(Framework& framework)
{
	unused(framework);
	return true;
}

bool FrameworkEntityBridgeTestCase::runTest(Framework& framework)
{
	unused(framework);

	bool runResult = true;
	EntityBridge& entityBridge = EntityBridge::get();
	EntityBridge::ObjectDesc entityObjectDesc = {};
	entityObjectDesc.dynamicProperty.entityIndex = 77;
	entityObjectDesc.dynamicProperty.hasTransform = true;
	entityObjectDesc.dynamicProperty.transform.positionX = 3.0f;

	auto entityHandle = entityBridge.createEntityHandle(entityObjectDesc);
	runResult = expectCondition(entityHandle.isValid(), "run: create entity bridge handle") && runResult;
	runResult = expectCondition(entityBridge.isHandleAlive(entityHandle), "run: created entity bridge handle alive") && runResult;
	if (!runResult)
	{
		return false;
	}

	const EntityBridge::DynamicData* dynamicData = entityBridge.getDynamicData(entityHandle);
	runResult = expectCondition(
		dynamicData != nullptr
			&& dynamicData->entityIndex == 77
			&& dynamicData->hasTransform
			&& dynamicData->transform.positionX == 3.0f,
		"run: initial dynamic data injected on create") && runResult;
	if (!runResult)
	{
		return false;
	}

	EntityBridge::DynamicData nextDynamicData = *dynamicData;
	nextDynamicData.entityIndex = 88;
	nextDynamicData.transform.positionX = 6.0f;
	entityBridge.updateDynamicData(entityHandle, nextDynamicData);

	dynamicData = entityBridge.getDynamicData(entityHandle);
	runResult = expectCondition(
		dynamicData != nullptr
			&& dynamicData->entityIndex == 77
			&& dynamicData->transform.positionX == 3.0f,
		"run: dynamic data update deferred before processFrame") && runResult;
	if (!runResult)
	{
		return false;
	}

	entityBridge.processFrame();
	dynamicData = entityBridge.getDynamicData(entityHandle);
	runResult = expectCondition(
		dynamicData != nullptr
			&& dynamicData->entityIndex == 88
			&& dynamicData->transform.positionX == 6.0f,
		"run: dynamic data update applied on processFrame") && runResult;
	if (!runResult)
	{
		return false;
	}

	const uint32 oldPackedHandle = entityHandle.getPackedHandle();
	entityHandle.reset();
	runResult = expectCondition(entityBridge.isHandleAlive(oldPackedHandle), "run: handle delete delayed before processFrame") && runResult;
	if (!runResult)
	{
		return false;
	}

	entityBridge.processFrame();
	runResult = expectCondition(!entityBridge.isHandleAlive(oldPackedHandle), "run: handle deleted on next processFrame") && runResult;
	return runResult;
}

bool FrameworkEntityBridgeTestCase::endTest(Framework& framework)
{
	unused(framework);
	return true;
}
