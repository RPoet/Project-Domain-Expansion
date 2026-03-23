#include "EngineTests/Framework/FrameworkTestCase.h"

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
