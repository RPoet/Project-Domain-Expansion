#pragma once

#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Framework/TestFramework.h"
#include "Engine/Framework/World.h"

class Framework
{
public:
	explicit Framework(FrameworkExecutionFlow executionFlow = FrameworkExecutionFlow::worldFlow);
	~Framework() = default;

	void setExecutionFlow(FrameworkExecutionFlow executionFlow);
	FrameworkExecutionFlow getExecutionFlow() const;

	uint32 createWorld(const wstring& worldName);
	bool loadWorld(uint32 worldIndex);
	bool changeWorld(uint32 worldIndex);
	bool unloadWorld(uint32 worldIndex);

	World* getWorld(uint32 worldIndex);
	const World* getWorld(uint32 worldIndex) const;
	World* getActiveWorld();
	const World* getActiveWorld() const;
	uint32 getActiveWorldIndex() const;

	bool tick(float deltaTimeSeconds);
	void registerTest();
	void addTestCase(unique_pointer<FrameworkTestCase> testCase);
	void clearTestCases();
	bool isTestFlowCompleted() const;
	const FrameworkTestSummary& getTestSummary() const;

private:
	bool isValidWorldIndex(uint32 worldIndex) const;

	FrameworkExecutionFlow executionFlow = FrameworkExecutionFlow::worldFlow;
	TestFramework testFramework;
	vector<unique_pointer<World>> worldStorage;
	uint32 activeWorldIndex = invalidWorldIndex;
};
