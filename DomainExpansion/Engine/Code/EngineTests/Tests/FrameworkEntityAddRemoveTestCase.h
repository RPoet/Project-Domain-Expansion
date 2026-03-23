#pragma once

#include "EngineTests/Framework/FrameworkTestCase.h"
#include "Engine/Framework/World.h"

class FrameworkEntityAddRemoveTestCase : public FrameworkTestCase
{
public:
	const char* getTestCaseName() const override;
	bool beginTest(Framework& framework) override;
	bool runTest(Framework& framework) override;
	bool endTest(Framework& framework) override;

private:
	World testWorld;
	uint32 rootEntityIndex = invalidEntityIndex;
	uint32 firstChildEntityIndex = invalidEntityIndex;
	uint32 secondChildEntityIndex = invalidEntityIndex;
	uint32 rootEntityBridgeHandle = uint32MaxValue;
	uint32 firstChildEntityBridgeHandle = uint32MaxValue;
	uint32 secondChildEntityBridgeHandle = uint32MaxValue;
	vector<uint32> movedComponentOwnerEntityIndexStorage;
	vector<uint32> movedComponentIndexStorage;
	uint32 movedComponentInitCount = 0;
};
