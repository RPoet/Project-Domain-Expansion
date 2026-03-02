#pragma once

#include "Engine/Framework/FrameworkTestCase.h"

unique_pointer<FrameworkTestCase> createFrameworkEntityAddRemoveTestCase();
unique_pointer<FrameworkTestCase> createFrameworkEntityUpdateTestCase();
unique_pointer<FrameworkTestCase> createFrameworkObjMeshLoaderTestCase();
unique_pointer<FrameworkTestCase> createFrameworkRootSignatureLifecycleTestCase();
unique_pointer<FrameworkTestCase> createFrameworkWorldSerializationTestCase();
