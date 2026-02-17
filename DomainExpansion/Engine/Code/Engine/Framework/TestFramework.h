#pragma once

#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Framework/FrameworkTestCase.h"

class Framework;

enum class FrameworkExecutionFlow : uint32
{
	worldFlow = 0,
	testFlow = 1,
	backendFlow = 2,
};

struct FrameworkTestSummary
{
	uint32 totalTestCaseCount = 0;
	uint32 passedTestCaseCount = 0;
	uint32 failedTestCaseCount = 0;
	uint32 totalAssertionCount = 0;
	uint32 failedAssertionCount = 0;
	bool completed = false;
};

class TestFramework
{
public:
	void addTestCase(unique_pointer<FrameworkTestCase> testCase);
	void clearTestCases();
	bool tick(Framework& framework);
	bool isCompleted() const;
	const FrameworkTestSummary& getSummary() const;

private:
	enum class TestPhase : uint32
	{
		beginPhase = 0,
		testPhase = 1,
		endPhase = 2,
	};

	vector<unique_pointer<FrameworkTestCase>> testCaseStorage;
	FrameworkTestSummary summary;
	uint32 activeTestCaseIndex = 0;
	TestPhase activeTestPhase = TestPhase::beginPhase;
	bool activeTestCaseFailed = false;
};
