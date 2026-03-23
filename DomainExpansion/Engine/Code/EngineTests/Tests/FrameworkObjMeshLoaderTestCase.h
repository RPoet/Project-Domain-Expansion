#pragma once

#include "EngineTests/Framework/FrameworkTestCase.h"

class FrameworkObjMeshLoaderTestCase : public FrameworkTestCase
{
public:
	const char* getTestCaseName() const override;
	bool beginTest(Framework& framework) override;
	bool runTest(Framework& framework) override;
	bool endTest(Framework& framework) override;

private:
	string sphereMeshPath = {};
	string planeMeshPath = {};
};
