#pragma once

#include "EngineTests/Framework/FrameworkTestCase.h"

class FrameworkInputModuleTestCase final : public FrameworkTestCase
{
public:
	const char* getTestCaseName() const override final;
	bool beginTest(Framework& framework) override final;
	bool runTest(Framework& framework) override final;
	bool endTest(Framework& framework) override final;
};
