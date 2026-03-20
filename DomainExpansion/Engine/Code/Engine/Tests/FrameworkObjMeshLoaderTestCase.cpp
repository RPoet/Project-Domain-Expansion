#include "Engine/Tests/FrameworkObjMeshLoaderTestCase.h"

#include "Engine/Framework/FrameworkFileSystem.h"
#include "Engine/Module/Asset/MeshParser.h"
#include "Engine/Module/Asset/MeshStreaming.h"

static bool resolveSphereMeshPath(string& outSphereMeshPath)
{
	return frameworkFileSystemResolvePathFromResources("Meshes/Sphere.obj", outSphereMeshPath);
}

static bool resolvePlaneMeshPath(string& outPlaneMeshPath)
{
	return frameworkFileSystemResolvePathFromResources("Meshes/Plane.obj", outPlaneMeshPath);
}

const char* FrameworkObjMeshLoaderTestCase::getTestCaseName() const
{
	return "FrameworkObjMeshLoaderTestCase";
}

bool FrameworkObjMeshLoaderTestCase::beginTest(Framework& framework)
{
	unused(framework);
	sphereMeshPath.clear();
	planeMeshPath.clear();
	shared_pointer<MeshStreaming> meshStreaming = MeshStreaming::get();
	bool beginResult = true;
	beginResult = expectCondition(
		resolveSphereMeshPath(sphereMeshPath),
		"begin: resolve Sphere.obj path") && beginResult;
	beginResult = expectCondition(
		resolvePlaneMeshPath(planeMeshPath),
		"begin: resolve Plane.obj path") && beginResult;
	beginResult = expectCondition(
		meshStreaming != nullptr,
		"begin: mesh streaming module available") && beginResult;
	if (meshStreaming != nullptr)
	{
		meshStreaming->clear();
	}

	return beginResult;
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

	MeshAsset planeMeshAsset = {};
	string planeErrorText = {};
	runResult = expectCondition(
		MeshParser::get().parseFromFile(planeMeshPath, 0, planeMeshAsset, planeErrorText),
		"run: load plane obj mesh") && runResult;
	runResult = expectCondition(
		planeMeshAsset.vertexCount == 20402
		&& planeMeshAsset.indexCount == 120000,
		"run: plane mesh has expected counts") && runResult;
	runResult = expectCondition(
		planeMeshAsset.vertexCount == static_cast<uint32>(planeMeshAsset.positionVertices.size())
		&& planeMeshAsset.vertexCount == static_cast<uint32>(planeMeshAsset.normalVertices.size())
		&& planeMeshAsset.vertexCount == static_cast<uint32>(planeMeshAsset.texcoordVertices.size())
		&& planeMeshAsset.indexCount == static_cast<uint32>(planeMeshAsset.indices.size()),
		"run: plane mesh counts match stream buffer sizes") && runResult;
	float minPlaneX = 0.0f;
	float maxPlaneX = 0.0f;
	float minPlaneY = 0.0f;
	float maxPlaneY = 0.0f;
	float minPlaneZ = 0.0f;
	float maxPlaneZ = 0.0f;
	if (!planeMeshAsset.positionVertices.empty())
	{
		minPlaneX = planeMeshAsset.positionVertices[0].x;
		maxPlaneX = planeMeshAsset.positionVertices[0].x;
		minPlaneY = planeMeshAsset.positionVertices[0].y;
		maxPlaneY = planeMeshAsset.positionVertices[0].y;
		minPlaneZ = planeMeshAsset.positionVertices[0].z;
		maxPlaneZ = planeMeshAsset.positionVertices[0].z;
		for (uint32 vertexIndex = 1; vertexIndex < planeMeshAsset.vertexCount; ++vertexIndex)
		{
			const MeshAsset::PositionData& positionVertex = planeMeshAsset.positionVertices[vertexIndex];
			if (positionVertex.x < minPlaneX)
			{
				minPlaneX = positionVertex.x;
			}
			if (positionVertex.x > maxPlaneX)
			{
				maxPlaneX = positionVertex.x;
			}
			if (positionVertex.y < minPlaneY)
			{
				minPlaneY = positionVertex.y;
			}
			if (positionVertex.y > maxPlaneY)
			{
				maxPlaneY = positionVertex.y;
			}
			if (positionVertex.z < minPlaneZ)
			{
				minPlaneZ = positionVertex.z;
			}
			if (positionVertex.z > maxPlaneZ)
			{
				maxPlaneZ = positionVertex.z;
			}
		}
	}
	runResult = expectCondition(
		!planeMeshAsset.positionVertices.empty()
		&& minPlaneX == -50.0f
		&& maxPlaneX == 50.0f
		&& minPlaneY == 0.0f
		&& maxPlaneY == 0.0f
		&& minPlaneZ == -50.0f
		&& maxPlaneZ == 50.0f,
		"run: plane mesh is centered on world origin with 100m extents on xz plane") && runResult;

	shared_pointer<MeshStreaming> meshStreaming = MeshStreaming::get();
	runResult = expectCondition(
		meshStreaming != nullptr,
		"run: mesh streaming module available") && runResult;
	if (meshStreaming != nullptr)
	{
		shared_pointer<MeshAssetHandle> planeMeshHandle = meshStreaming->requestMesh("Meshes/Plane.obj", 0);
		runResult = expectCondition(
			planeMeshHandle != nullptr,
			"run: request plane mesh through streaming") && runResult;
		meshStreaming->postUpdate();
		runResult = expectCondition(
			planeMeshHandle != nullptr
			&& planeMeshHandle->state == MeshAssetHandleState::ready
			&& planeMeshHandle->gpuState == MeshAssetGpuState::pending
			&& planeMeshHandle->meshAsset != nullptr,
			"run: plane mesh streaming cpu load succeeds") && runResult;
		if (planeMeshHandle != nullptr && planeMeshHandle->meshAsset != nullptr)
		{
			runResult = expectCondition(
				planeMeshHandle->meshAsset->vertexCount == 20402
				&& planeMeshHandle->meshAsset->indexCount == 120000,
				"run: plane mesh streaming preserves expected counts") && runResult;
		}
	}

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
	planeMeshPath.clear();
	shared_pointer<MeshStreaming> meshStreaming = MeshStreaming::get();
	if (meshStreaming != nullptr)
	{
		meshStreaming->clear();
	}

	return expectCondition(
		true,
		"end: obj mesh loader test cleanup");
}
