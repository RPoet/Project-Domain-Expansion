#include "Engine/Framework/TestFramework.h"
#include "Engine/Framework/Framework.h"

void TestFramework::addTestCase(unique_pointer<FrameworkTestCase> testCase)
{
	if (testCase == nullptr)
	{
		return;
	}

	testCaseStorage.push_back(moveValue(testCase));
	summary.totalTestCaseCount = static_cast<uint32>(testCaseStorage.size());
}

void TestFramework::clearTestCases()
{
	testCaseStorage.clear();
	summary = {};
	activeTestCaseIndex = 0;
	activeTestPhase = TestPhase::beginPhase;
	activeTestCaseFailed = false;
}

bool TestFramework::tick(Framework& framework)
{
	if (summary.completed)
	{
		return true;
	}

	if (testCaseStorage.empty())
	{
		summary.completed = true;
		output << "[FrameworkTest] No test case inserted." << lineBreak;
		return true;
	}

	if (activeTestCaseIndex >= static_cast<uint32>(testCaseStorage.size()))
	{
		summary.completed = true;
		output << "[FrameworkTest] Completed. passedCase=" << summary.passedTestCaseCount
			  << ", failedCase=" << summary.failedTestCaseCount
			  << ", failedAssertion=" << summary.failedAssertionCount
			  << ", totalAssertion=" << summary.totalAssertionCount << lineBreak;
		return true;
	}

	FrameworkTestCase* activeTestCase = testCaseStorage[activeTestCaseIndex].get();
	if (activeTestCase == nullptr)
	{
		++summary.failedTestCaseCount;
		++activeTestCaseIndex;
		activeTestPhase = TestPhase::beginPhase;
		activeTestCaseFailed = true;
		return true;
	}

	if (activeTestPhase == TestPhase::beginPhase)
	{
		activeTestCase->resetAssertions();
		activeTestCaseFailed = !activeTestCase->beginTest(framework);
		activeTestPhase = TestPhase::testPhase;
		return true;
	}

	if (activeTestPhase == TestPhase::testPhase)
	{
		if (!activeTestCase->runTest(framework))
		{
			activeTestCaseFailed = true;
		}

		activeTestPhase = TestPhase::endPhase;
		return true;
	}

	if (!activeTestCase->endTest(framework))
	{
		activeTestCaseFailed = true;
	}

	summary.totalAssertionCount += activeTestCase->getTotalAssertionCount();
	summary.failedAssertionCount += activeTestCase->getFailedAssertionCount();
	if (activeTestCase->getFailedAssertionCount() > 0)
	{
		activeTestCaseFailed = true;
	}

	if (activeTestCaseFailed)
	{
		++summary.failedTestCaseCount;
		error << "[FrameworkTest][CaseFail] " << activeTestCase->getTestCaseName() << lineBreak;
	}
	else
	{
		++summary.passedTestCaseCount;
		output << "[FrameworkTest][CasePass] " << activeTestCase->getTestCaseName() << lineBreak;
	}

	++activeTestCaseIndex;
	activeTestPhase = TestPhase::beginPhase;
	activeTestCaseFailed = false;

	return true;
}

bool TestFramework::isCompleted() const
{
	return summary.completed;
}

const FrameworkTestSummary& TestFramework::getSummary() const
{
	return summary;
}
