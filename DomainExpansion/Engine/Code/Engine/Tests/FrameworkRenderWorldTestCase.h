#pragma once

#include "Engine/Framework/FrameworkTestCase.h"

class FrameworkRenderWorldTestCase final : public FrameworkTestCase
{
public:
	const char* getTestCaseName() const override;
	bool beginTest(Framework& framework) override;
	bool runTest(Framework& framework) override;
	bool endTest(Framework& framework) override;

private:
	uint32 worldIndex = invalidWorldIndex;
	uint32 entityIndex = invalidEntityIndex;
};
