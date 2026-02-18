#include "Engine/Tests/EntityTest.h"
#include "Engine/Tests/FrameworkEntityAddRemoveTestCase.h"
#include "Engine/Tests/FrameworkEntityUpdateTestCase.h"
#include "Engine/Tests/FrameworkObjMeshLoaderTestCase.h"
#include "Engine/Tests/FrameworkWorldSerializationTestCase.h"

unique_pointer<FrameworkTestCase> createFrameworkEntityAddRemoveTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkEntityAddRemoveTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkEntityUpdateTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkEntityUpdateTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkObjMeshLoaderTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkObjMeshLoaderTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkWorldSerializationTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkWorldSerializationTestCase());
}
