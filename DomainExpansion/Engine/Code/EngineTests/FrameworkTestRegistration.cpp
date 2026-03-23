#include "EngineTests/FrameworkTestRegistration.h"

#include "EngineTests/Tests/EntityTest.h"
#include "EngineTests/Framework/TestFramework.h"

void registerFrameworkTests(TestFramework& testFramework)
{
	testFramework.clearTestCases();
	testFramework.addTestCase(createFrameworkEntityAddRemoveTestCase());
	testFramework.addTestCase(createFrameworkEntityUpdateTestCase());
	testFramework.addTestCase(createFrameworkEntityBridgeTestCase());
	testFramework.addTestCase(createFrameworkEntityBridgeSyncTestCase());
	testFramework.addTestCase(createFrameworkCLIModuleTestCase());
	testFramework.addTestCase(createFrameworkInputModuleTestCase());
	testFramework.addTestCase(createFrameworkCameraBridgeTestCase());
	testFramework.addTestCase(createFrameworkRenderWorldTestCase());
	testFramework.addTestCase(createFrameworkBridgeLifecycleTestCase());
	testFramework.addTestCase(createFrameworkObjMeshLoaderTestCase());
	testFramework.addTestCase(createFrameworkRootSignatureLifecycleTestCase());
	testFramework.addTestCase(createFrameworkPipelineStateLifecycleTestCase());
	testFramework.addTestCase(createFrameworkShaderPackageTestCase());
	testFramework.addTestCase(createFrameworkShaderAssetSmokeTestCase());
	testFramework.addTestCase(createFrameworkWorldSerializationTestCase());
}
