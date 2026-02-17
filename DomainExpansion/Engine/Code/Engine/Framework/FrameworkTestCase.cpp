#include "Engine/Framework/FrameworkTestCase.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Tests/EntityTest.h"

void FrameworkTestCase::resetAssertions()
{
	totalAssertionCount = 0;
	failedAssertionCount = 0;
}

uint32 FrameworkTestCase::getTotalAssertionCount() const
{
	return totalAssertionCount;
}

uint32 FrameworkTestCase::getFailedAssertionCount() const
{
	return failedAssertionCount;
}

bool FrameworkTestCase::expectCondition(const bool condition, const char* conditionName)
{
	++totalAssertionCount;
	if (condition)
	{
		output << "[FrameworkTest][Pass][" << getTestCaseName() << "] " << conditionName << lineBreak;
		return true;
	}

	++failedAssertionCount;
	error << "[FrameworkTest][Fail][" << getTestCaseName() << "] " << conditionName << lineBreak;
	return false;
}

void Framework::registerTest()
{
	clearTestCases();
	addTestCase(createFrameworkEntityAddRemoveTestCase());
	addTestCase(createFrameworkEntityUpdateTestCase());
}
