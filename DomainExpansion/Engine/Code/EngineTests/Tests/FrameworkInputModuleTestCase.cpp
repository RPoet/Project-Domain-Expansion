#include "EngineTests/Tests/FrameworkInputModuleTestCase.h"

#include "Engine/Module/Input/InputModule.h"

static bool isValidInputKeyState(const InputKeyState keyState)
{
	switch (keyState)
	{
	case InputKeyState::up:
	case InputKeyState::pressed:
	case InputKeyState::down:
	case InputKeyState::released:
		return true;
	default:
		return false;
	}
}

const char* FrameworkInputModuleTestCase::getTestCaseName() const
{
	return "FrameworkInputModuleTestCase";
}

bool FrameworkInputModuleTestCase::beginTest(Framework& framework)
{
	unused(framework);
	return expectCondition(InputModule::get() != nullptr, "begin: input module exists");
}

bool FrameworkInputModuleTestCase::runTest(Framework& framework)
{
	unused(framework);

	shared_pointer<InputModule> inputModule = InputModule::get();
	int2 mousePosition = {};
	int2 mousePositionDelta = {};
	int2 mouseScrollDelta = {};
	if (inputModule != nullptr)
	{
		mousePosition = inputModule->getMousePosition();
		mousePositionDelta = inputModule->getMousePositionDelta();
		mouseScrollDelta = inputModule->getMouseScrollDelta();
	}

	unused(mousePosition);
	unused(mousePositionDelta);
	unused(mouseScrollDelta);

	bool runResult = true;
	runResult = expectCondition(
		inputModule != nullptr,
		"run: input module exists") && runResult;
	runResult = expectCondition(
		inputModule != nullptr && isValidInputKeyState(inputModule->getKeyState('A')),
		"run: key state returns a valid enum value") && runResult;
	runResult = expectCondition(
		inputModule != nullptr && isValidInputKeyState(inputModule->getMouseButtonState(InputMouseButton::left)),
		"run: mouse button state returns a valid enum value") && runResult;
	runResult = expectCondition(
		inputModule != nullptr,
		"run: mouse position accessors are callable") && runResult;
	runResult = expectCondition(
		inputModule != nullptr,
		"run: mouse delta accessors are callable") && runResult;
	runResult = expectCondition(
		inputModule != nullptr,
		"run: mouse scroll accessors are callable") && runResult;
	return runResult;
}

bool FrameworkInputModuleTestCase::endTest(Framework& framework)
{
	unused(framework);
	return true;
}
