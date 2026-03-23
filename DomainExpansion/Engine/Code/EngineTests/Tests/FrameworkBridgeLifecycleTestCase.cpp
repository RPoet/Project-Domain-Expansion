#include "EngineTests/Tests/FrameworkBridgeLifecycleTestCase.h"

#include "Bridge/DefaultBridge.h"

struct FrameworkBridgeLifecycleObject
{
	struct StaticProperty
	{
		uint32 staticValue = 0;
	};

	struct DynamicProperty
	{
		uint32 dynamicValue = 0;
	};

	struct ObjectDesc
	{
		StaticProperty staticProperty = {};
		DynamicProperty dynamicProperty = {};
	};
};

const char* FrameworkBridgeLifecycleTestCase::getTestCaseName() const
{
	return "FrameworkBridgeLifecycleTestCase";
}

bool FrameworkBridgeLifecycleTestCase::beginTest(Framework& framework)
{
	unused(framework);
	return true;
}

bool FrameworkBridgeLifecycleTestCase::runTest(Framework& framework)
{
	unused(framework);

	bool runResult = true;
	DefaultBridge<FrameworkBridgeLifecycleObject, 16> bridge = {};

	FrameworkBridgeLifecycleObject::ObjectDesc objectDesc = {};
	objectDesc.staticProperty.staticValue = 11;
	objectDesc.dynamicProperty.dynamicValue = 21;
	auto handleReference = bridge.createObject(objectDesc);
	runResult = expectCondition(handleReference.isValid(), "run: create handle is valid") && runResult;
	runResult = expectCondition(bridge.isHandleAlive(handleReference), "run: created handle is alive") && runResult;
	if (!runResult)
	{
		return false;
	}

	const FrameworkBridgeLifecycleObject::StaticProperty* staticProperty = bridge.getStaticProperty(handleReference);
	runResult = expectCondition(
		staticProperty != nullptr && staticProperty->staticValue == 11,
		"run: static property stored") && runResult;
	if (!runResult)
	{
		return false;
	}

	FrameworkBridgeLifecycleObject::DynamicProperty dynamicProperty = {};
	dynamicProperty.dynamicValue = 77;
	bridge.updateObject(handleReference, dynamicProperty);
	const FrameworkBridgeLifecycleObject::DynamicProperty* dynamicPropertyBeforeFlush = bridge.getDynamicProperty(handleReference);
	runResult = expectCondition(
		dynamicPropertyBeforeFlush != nullptr && dynamicPropertyBeforeFlush->dynamicValue == 21,
		"run: dynamic update deferred before frame process") && runResult;
	if (!runResult)
	{
		return false;
	}

	bridge.processFrame();
	const FrameworkBridgeLifecycleObject::DynamicProperty* dynamicPropertyAfterFlush = bridge.getDynamicProperty(handleReference);
	runResult = expectCondition(
		dynamicPropertyAfterFlush != nullptr && dynamicPropertyAfterFlush->dynamicValue == 77,
		"run: dynamic update applied after frame process") && runResult;
	if (!runResult)
	{
		return false;
	}

	const uint32 oldPackedHandle = handleReference.getPackedHandle();
	const uint32 oldIndex = handleReference.getIndex();
	const uint32 oldGeneration = handleReference.getGeneration();
	handleReference.reset();
	runResult = expectCondition(
		!handleReference.isValid(),
		"run: handle reset invalidates reference") && runResult;
	runResult = expectCondition(
		bridge.isHandleAlive(oldPackedHandle),
		"run: delete is delayed by one frame") && runResult;
	if (!runResult)
	{
		return false;
	}

	bridge.processFrame();
	runResult = expectCondition(
		!bridge.isHandleAlive(oldPackedHandle),
		"run: delayed delete applied on next frame") && runResult;
	if (!runResult)
	{
		return false;
	}

	auto reusedHandleReference = bridge.createObject(objectDesc);
	runResult = expectCondition(
		reusedHandleReference.isValid(),
		"run: create handle after delete succeeds") && runResult;
	runResult = expectCondition(
		reusedHandleReference.getIndex() == oldIndex,
		"run: freed index reused") && runResult;
	runResult = expectCondition(
		reusedHandleReference.getGeneration() != oldGeneration,
		"run: generation changed on index reuse") && runResult;
	runResult = expectCondition(
		!bridge.isHandleAlive(oldPackedHandle),
		"run: stale packed handle remains invalid") && runResult;
	return runResult;
}

bool FrameworkBridgeLifecycleTestCase::endTest(Framework& framework)
{
	unused(framework);
	return true;
}
