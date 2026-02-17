#pragma once

#include "Engine/Framework/FrameworkConstants.h"

class Framework;

class FrameworkTestCase
{
public:
	virtual ~FrameworkTestCase() = default;

	virtual const char* getTestCaseName() const = 0;
	virtual bool beginTest(Framework& framework) = 0;
	virtual bool runTest(Framework& framework) = 0;
	virtual bool endTest(Framework& framework) = 0;

	void resetAssertions();
	uint32 getTotalAssertionCount() const;
	uint32 getFailedAssertionCount() const;

protected:
	bool expectCondition(bool condition, const char* conditionName);

private:
	uint32 totalAssertionCount = 0;
	uint32 failedAssertionCount = 0;
};
