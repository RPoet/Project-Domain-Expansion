#pragma once

#include "EngineTests/Framework/FrameworkTestCase.h"
#include "Engine/Framework/World.h"

class FrameworkEntityUpdateTestCase : public FrameworkTestCase
{
public:
	const char* getTestCaseName() const override;
	bool beginTest(Framework& framework) override;
	bool runTest(Framework& framework) override;
	bool endTest(Framework& framework) override;

private:
	World testWorld;
	uint32 parentEntityIndex = invalidEntityIndex;
	uint32 childEntityIndex = invalidEntityIndex;
	uint32 childComponentIndex = invalidComponentIndex;
	uint32 parentComponentIndex = invalidComponentIndex;
	vector<uint32> updateOrderStorage;
	vector<uint32> childInitEntityIndexStorage;
	vector<uint32> childInitComponentIndexStorage;
	uint32 parentTickCount = 0;
	uint32 childTickCount = 0;
	uint32 childInitCount = 0;
};
