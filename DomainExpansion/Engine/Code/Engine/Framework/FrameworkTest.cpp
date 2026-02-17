#include "Engine/Framework/Framework.h"
#include "Engine/Tests/EntityTest.h"

void Framework::registerTest()
{
	clearTestCases();
	addTestCase(createFrameworkEntityAddRemoveTestCase());
	addTestCase(createFrameworkEntityUpdateTestCase());
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

	return executionCompleted || testFramework.isCompleted();
}

const FrameworkTestSummary& Framework::getTestSummary() const
{
	return testFramework.getSummary();
}

void Framework::finalizeTestFlow()
{
	if (executionCompleted)
	{
		return;
	}

	const FrameworkTestSummary& frameworkTestSummary = testFramework.getSummary();
	if (frameworkTestSummary.failedTestCaseCount == 0)
	{
		output << "Framework tests completed successfully. passedCase="
			   << frameworkTestSummary.passedTestCaseCount
			   << ", totalCase=" << frameworkTestSummary.totalTestCaseCount << lineBreak;
	}
	else
	{
		error << "Framework tests failed. failedCase=" << frameworkTestSummary.failedTestCaseCount
			  << ", failedAssertion=" << frameworkTestSummary.failedAssertionCount
			  << ", totalAssertion=" << frameworkTestSummary.totalAssertionCount << lineBreak;
	}

	executionCompleted = true;
	runtimeExitCode = 0;
}
