#include "Engine/Tests/FrameworkObjMeshLoaderTestCase.h"

#include "Engine/Framework/FrameworkFileSystem.h"
#include "Engine/Module/Asset/MeshParser.h"

static bool resolveSphereMeshPath(string& outSphereMeshPath)
{
	return frameworkFileSystemResolvePathFromResources("Meshes/Sphere.obj", outSphereMeshPath);
}

const char* FrameworkObjMeshLoaderTestCase::getTestCaseName() const
{
	return "FrameworkObjMeshLoaderTestCase";
}

bool FrameworkObjMeshLoaderTestCase::beginTest(Framework& framework)
{
	unused(framework);
	sphereMeshPath.clear();
	return expectCondition(
		resolveSphereMeshPath(sphereMeshPath),
		"begin: resolve Sphere.obj path");
}

bool FrameworkObjMeshLoaderTestCase::runTest(Framework& framework)
{
	unused(framework);

	MeshAsset meshAsset = {};
	string errorText = {};
	bool runResult = true;
	runResult = expectCondition(
		MeshParser::get().parseFromFile(sphereMeshPath, 0, meshAsset, errorText),
		"run: load sphere obj mesh") && runResult;
	runResult = expectCondition(
		meshAsset.vertexCount > 0 && meshAsset.indexCount > 0,
		"run: loaded mesh has vertices and indices") && runResult;
	runResult = expectCondition(
		meshAsset.vertexCount == static_cast<uint32>(meshAsset.positionVertices.size())
		&& meshAsset.vertexCount == static_cast<uint32>(meshAsset.normalVertices.size())
		&& meshAsset.vertexCount == static_cast<uint32>(meshAsset.texcoordVertices.size())
		&& meshAsset.indexCount == static_cast<uint32>(meshAsset.indices.size()),
		"run: mesh counts match stream buffer sizes") && runResult;

	MeshAsset missingMeshAsset = {};
	string missingErrorText = {};
	runResult = expectCondition(
		!MeshParser::get().parseFromFile("Engine/Resources/Meshes/NotFound.obj", 0, missingMeshAsset, missingErrorText),
		"run: missing obj mesh load fails") && runResult;

	return runResult;
}

bool FrameworkObjMeshLoaderTestCase::endTest(Framework& framework)
{
	unused(framework);
	sphereMeshPath.clear();
	return expectCondition(
		true,
		"end: obj mesh loader test cleanup");
}
