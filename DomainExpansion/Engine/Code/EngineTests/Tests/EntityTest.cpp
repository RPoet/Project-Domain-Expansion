#include "EngineTests/Tests/EntityTest.h"
#include "EngineTests/Tests/FrameworkBridgeLifecycleTestCase.h"
#include "EngineTests/Tests/FrameworkCLIModuleTestCase.h"
#include "EngineTests/Tests/FrameworkEntityBridgeTestCase.h"
#include "EngineTests/Tests/FrameworkEntityBridgeSyncTestCase.h"
#include "EngineTests/Tests/FrameworkInputModuleTestCase.h"
#include "EngineTests/Tests/FrameworkCameraBridgeTestCase.h"
#include "EngineTests/Tests/FrameworkEntityAddRemoveTestCase.h"
#include "EngineTests/Tests/FrameworkRenderWorldTestCase.h"
#include "EngineTests/Tests/FrameworkEntityUpdateTestCase.h"
#include "EngineTests/Tests/FrameworkObjMeshLoaderTestCase.h"
#include "EngineTests/Tests/FrameworkPipelineStateLifecycleTestCase.h"
#include "EngineTests/Tests/FrameworkRootSignatureLifecycleTestCase.h"
#include "EngineTests/Tests/FrameworkShaderAssetSmokeTestCase.h"
#include "EngineTests/Tests/FrameworkShaderPackageTestCase.h"
#include "EngineTests/Tests/FrameworkWorldSerializationTestCase.h"

unique_pointer<FrameworkTestCase> createFrameworkEntityAddRemoveTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkEntityAddRemoveTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkEntityUpdateTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkEntityUpdateTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkEntityBridgeTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkEntityBridgeTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkEntityBridgeSyncTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkEntityBridgeSyncTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkCLIModuleTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkCLIModuleTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkInputModuleTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkInputModuleTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkCameraBridgeTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkCameraBridgeTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkRenderWorldTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkRenderWorldTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkBridgeLifecycleTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkBridgeLifecycleTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkObjMeshLoaderTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkObjMeshLoaderTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkRootSignatureLifecycleTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkRootSignatureLifecycleTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkPipelineStateLifecycleTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkPipelineStateLifecycleTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkShaderPackageTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkShaderPackageTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkShaderAssetSmokeTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkShaderAssetSmokeTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkWorldSerializationTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkWorldSerializationTestCase());
}
