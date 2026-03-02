#include "Engine/Framework/Framework.h"
#include "Engine/Tests/EntityTest.h"

void Framework::registerTest()
{
	clearTestCases();
	addTestCase(createFrameworkEntityAddRemoveTestCase());
	addTestCase(createFrameworkEntityUpdateTestCase());
	addTestCase(createFrameworkObjMeshLoaderTestCase());
	addTestCase(createFrameworkRootSignatureLifecycleTestCase());
	addTestCase(createFrameworkShaderPackageTestCase());
	addTestCase(createFrameworkWorldSerializationTestCase());
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

bool Framework::updateTestExecutionFlow()
{
	preUpdateModules();
	const bool updateResult = tickTestFlow();
	postUpdateModules();
	return updateResult;
}

bool Framework::tickTestFlow()
{
	const bool tickResult = testFramework.tick(*this);
	if (!tickResult)
	{
		runtimeExitCode = FrameworkRuntimeExitCode::testFlowTickFailed;
		executionCompleted = true;
		return false;
	}

	if (testFramework.isCompleted())
	{
		finalizeTestFlow();
	}

	return true;
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
	runtimeExitCode = FrameworkRuntimeExitCode::success;
}
