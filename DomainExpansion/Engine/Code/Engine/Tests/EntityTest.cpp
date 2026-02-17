#include "Engine/Tests/EntityTest.h"
#include "Engine/Tests/FrameworkEntityAddRemoveTestCase.h"
#include "Engine/Tests/FrameworkEntityUpdateTestCase.h"

unique_pointer<FrameworkTestCase> createFrameworkEntityAddRemoveTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkEntityAddRemoveTestCase());
}

unique_pointer<FrameworkTestCase> createFrameworkEntityUpdateTestCase()
{
	return unique_pointer<FrameworkTestCase>(new FrameworkEntityUpdateTestCase());
}

