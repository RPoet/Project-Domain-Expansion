#pragma once

#include "Engine/Framework/FrameworkTestCase.h"

class FrameworkBridgeLifecycleTestCase final : public FrameworkTestCase
{
public:
	const char* getTestCaseName() const override;
	bool beginTest(Framework& framework) override;
	bool runTest(Framework& framework) override;
	bool endTest(Framework& framework) override;
};
