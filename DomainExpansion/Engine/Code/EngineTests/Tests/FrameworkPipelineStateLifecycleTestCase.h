#pragma once

#include "EngineTests/Framework/FrameworkTestCase.h"
#include "Render/Backends/RenderBackend.h"

class FrameworkPipelineStateLifecycleTestCase : public FrameworkTestCase
{
public:
	const char* getTestCaseName() const override;
	bool beginTest(Framework& framework) override;
	bool runTest(Framework& framework) override;
	bool endTest(Framework& framework) override;

private:
	unique_pointer<RenderBackend> testRenderBackend = nullptr;
};
