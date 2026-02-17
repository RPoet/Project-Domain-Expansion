#include "Engine/Framework/Framework.h"

Framework::Framework(const FrameworkExecutionFlow executionFlow)
	: executionFlow(executionFlow)
{
}

void Framework::setExecutionFlow(const FrameworkExecutionFlow executionFlow)
{
	this->executionFlow = executionFlow;
}

FrameworkExecutionFlow Framework::getExecutionFlow() const
{
	return executionFlow;
}

uint32 Framework::createWorld(const wstring& worldName)
{
	unique_pointer<World> worldInstance(new World(worldName));
	worldStorage.push_back(moveValue(worldInstance));
	return static_cast<uint32>(worldStorage.size() - 1);
}

bool Framework::loadWorld(const uint32 worldIndex)
{
	if (!isValidWorldIndex(worldIndex))
	{
		return false;
	}

	if (worldStorage[worldIndex] == nullptr)
	{
		return false;
	}

	activeWorldIndex = worldIndex;
	return true;
}

bool Framework::changeWorld(const uint32 worldIndex)
{
	return loadWorld(worldIndex);
}

bool Framework::unloadWorld(const uint32 worldIndex)
{
	if (!isValidWorldIndex(worldIndex))
	{
		return false;
	}

	worldStorage[worldIndex].reset();
	if (activeWorldIndex == worldIndex)
	{
		activeWorldIndex = invalidWorldIndex;
	}

	return true;
}

World* Framework::getWorld(const uint32 worldIndex)
{
	if (!isValidWorldIndex(worldIndex))
	{
		return nullptr;
	}

	return worldStorage[worldIndex].get();
}

const World* Framework::getWorld(const uint32 worldIndex) const
{
	if (!isValidWorldIndex(worldIndex))
	{
		return nullptr;
	}

	return worldStorage[worldIndex].get();
}

World* Framework::getActiveWorld()
{
	return getWorld(activeWorldIndex);
}

const World* Framework::getActiveWorld() const
{
	return getWorld(activeWorldIndex);
}

uint32 Framework::getActiveWorldIndex() const
{
	return activeWorldIndex;
}

bool Framework::tick(const float deltaTimeSeconds)
{
	if (executionFlow == FrameworkExecutionFlow::testFlow)
	{
		unused(deltaTimeSeconds);
		return testFramework.tick(*this);
	}

	World* activeWorldObject = getActiveWorld();
	if (activeWorldObject == nullptr)
	{
		return false;
	}

	return activeWorldObject->tick(deltaTimeSeconds);
}

void Framework::addTestCase(unique_pointer<FrameworkTestCase> testCase)
{
	testFramework.addTestCase(moveValue(testCase));
}

void Framework::clearTestCases()
{
	testFramework.clearTestCases();
}

bool Framework::isTestFlowCompleted() const
{
	if (executionFlow != FrameworkExecutionFlow::testFlow)
	{
		return false;
	}

	return testFramework.isCompleted();
}

const FrameworkTestSummary& Framework::getTestSummary() const
{
	return testFramework.getSummary();
}

bool Framework::isValidWorldIndex(const uint32 worldIndex) const
{
	return worldIndex < static_cast<uint32>(worldStorage.size());
}
