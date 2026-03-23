#pragma once

#include "Bridge/BridgeHandle.h"
#include "EngineTests/Framework/FrameworkTestCase.h"

class FrameworkEntityBridgeSyncTestCase : public FrameworkTestCase
{
public:
	const char* getTestCaseName() const override;
	bool beginTest(Framework& framework) override;
	bool runTest(Framework& framework) override;
	bool endTest(Framework& framework) override;

private:
	uint32 worldIndex = invalidWorldIndex;
	uint32 entityIndex = invalidEntityIndex;
	BridgeHandle entityHandle = invalidBridgeHandle;
	BridgeHandle meshHandle = invalidBridgeHandle;
	BridgeHandle cameraHandle = invalidBridgeHandle;
};
