#include "Engine/Tests/EntityTest.h"
#include "Engine/Tests/FrameworkBridgeLifecycleTestCase.h"
#include "Engine/Tests/FrameworkEntityBridgeTestCase.h"
#include "Engine/Tests/FrameworkEntityBridgeSyncTestCase.h"
#include "Engine/Tests/FrameworkEntityAddRemoveTestCase.h"
#include "Engine/Tests/FrameworkEntityUpdateTestCase.h"
#include "Engine/Tests/FrameworkObjMeshLoaderTestCase.h"
#include "Engine/Tests/FrameworkPipelineStateLifecycleTestCase.h"
#include "Engine/Tests/FrameworkRootSignatureLifecycleTestCase.h"
#include "Engine/Tests/FrameworkShaderPackageTestCase.h"
#include "Engine/Tests/FrameworkWorldSerializationTestCase.h"

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

unique_pointer<FrameworkTestCase> createFrameworkWorldSerializationTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkWorldSerializationTestCase());
}
