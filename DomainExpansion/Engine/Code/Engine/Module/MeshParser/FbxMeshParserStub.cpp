#include "Engine/Module/MeshParser/FbxMeshParserStub.h"

#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/World.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/MeshParser/FbxBinaryParser.h"

static constexpr float fbxDegreesToRadians = 3.1415926535f / 180.0f;

struct FbxVertexKey
{
	uint32 controlPointIndex = 0;
	uint32 normalIndex = uint32MaxValue;
	uint32 texcoordIndex = uint32MaxValue;

	bool operator==(const FbxVertexKey& other) const
	{
		return controlPointIndex == other.controlPointIndex
			&& normalIndex == other.normalIndex
			&& texcoordIndex == other.texcoordIndex;
	}
};

struct FbxVertexKeyHasher
{
	size_t operator()(const FbxVertexKey& value) const
	{
		size_t hashValue = static_cast<size_t>(value.controlPointIndex);
		hashValue = (hashValue * 16777619u) ^ static_cast<size_t>(value.normalIndex);
		hashValue = (hashValue * 16777619u) ^ static_cast<size_t>(value.texcoordIndex);
		return hashValue;
	}
};

struct float3x3
{
	float value[9] = {};
};

struct FbxImportSectionSource
{
	uint32 polygonStartIndex = 0;
	uint32 polygonCount = 0;
	int32 materialSlotIndex = -1;
	FbxObjectIdentifier materialIdentifier = 0;
};

struct FbxImportMeshNode
{
	const FbxGeometryData* geometryData = nullptr;
	FbxObjectIdentifier geometryIdentifier = 0;
	string geometryName = {};
	FbxObjectIdentifier modelIdentifier = 0;
	string modelName = {};
	float4x4 worldMatrix = {};
	vector<FbxObjectIdentifier> materialIdentifiers = {};
	vector<FbxImportSectionSource> sections = {};
};

static bool failFbxMeshBuild(
	const string& meshFilePath,
	const string& reason,
	string& outErrorText)
{
	outErrorText = reason;
	error << "[MeshParser][Error] path=" << meshFilePath
		  << " reason=" << outErrorText << lineBreak;
	return false;
}

static float toFbxRadians(const float degrees)
{
	return degrees * fbxDegreesToRadians;
}

static float4x4 buildFbxModelLocalMatrix(const FbxModelData& modelData)
{
	float3 rotationInRadians = {};
	rotationInRadians.x = toFbxRadians(modelData.rotationInDegrees.x);
	rotationInRadians.y = toFbxRadians(modelData.rotationInDegrees.y);
	rotationInRadians.z = toFbxRadians(modelData.rotationInDegrees.z);
	return buildWorldMatrix4x4(modelData.translation, rotationInRadians, modelData.scaling);
}

static bool buildFbxModelWorldMatrices(
	const FbxSceneData& sceneData,
	unordered_map<FbxObjectIdentifier, float4x4>& outModelWorldMatrices)
{
	outModelWorldMatrices.clear();
	auto buildModelWorldMatrix = [&](const auto& recursiveBuild, const FbxObjectIdentifier modelIdentifier, float4x4& outWorldMatrix) -> bool
	{
		const auto foundCachedMatrix = outModelWorldMatrices.find(modelIdentifier);
		if (foundCachedMatrix != outModelWorldMatrices.end())
		{
			outWorldMatrix = foundCachedMatrix->second;
			return true;
		}

		const auto foundModel = sceneData.modelByIdentifier.find(modelIdentifier);
		if (foundModel == sceneData.modelByIdentifier.end())
		{
			return false;
		}

		const float4x4 localMatrix = buildFbxModelLocalMatrix(foundModel->second);
		outWorldMatrix = localMatrix;
		const FbxObjectIdentifier parentModelIdentifier = foundModel->second.parentModelIdentifier;
		if (parentModelIdentifier != 0 && sceneData.modelByIdentifier.find(parentModelIdentifier) != sceneData.modelByIdentifier.end())
		{
			float4x4 parentWorldMatrix = {};
			if (!recursiveBuild(recursiveBuild, parentModelIdentifier, parentWorldMatrix))
			{
				return false;
			}

			outWorldMatrix = multiplyMatrix4x4(localMatrix, parentWorldMatrix);
		}

		outModelWorldMatrices.emplace(modelIdentifier, outWorldMatrix);
		return true;
	};

	for (auto modelIterator = sceneData.modelByIdentifier.begin();
		modelIterator != sceneData.modelByIdentifier.end();
		++modelIterator)
	{
		float4x4 worldMatrix = {};
		if (!buildModelWorldMatrix(buildModelWorldMatrix, modelIterator->first, worldMatrix))
		{
			return false;
		}
	}

	return true;
}

static bool buildInverseTranspose3x3(const float4x4& matrix, float3x3& outMatrix)
{
	const float a00 = matrix.value[0];
	const float a01 = matrix.value[1];
	const float a02 = matrix.value[2];
	const float a10 = matrix.value[4];
	const float a11 = matrix.value[5];
	const float a12 = matrix.value[6];
	const float a20 = matrix.value[8];
	const float a21 = matrix.value[9];
	const float a22 = matrix.value[10];

	const float determinant =
		a00 * (a11 * a22 - a12 * a21)
		- a01 * (a10 * a22 - a12 * a20)
		+ a02 * (a10 * a21 - a11 * a20);
	if (determinant > -0.000001f && determinant < 0.000001f)
	{
		return false;
	}

	const float inverseDeterminant = 1.0f / determinant;
	const float inverse00 = (a11 * a22 - a12 * a21) * inverseDeterminant;
	const float inverse01 = (a02 * a21 - a01 * a22) * inverseDeterminant;
	const float inverse02 = (a01 * a12 - a02 * a11) * inverseDeterminant;
	const float inverse10 = (a12 * a20 - a10 * a22) * inverseDeterminant;
	const float inverse11 = (a00 * a22 - a02 * a20) * inverseDeterminant;
	const float inverse12 = (a02 * a10 - a00 * a12) * inverseDeterminant;
	const float inverse20 = (a10 * a21 - a11 * a20) * inverseDeterminant;
	const float inverse21 = (a01 * a20 - a00 * a21) * inverseDeterminant;
	const float inverse22 = (a00 * a11 - a01 * a10) * inverseDeterminant;

	outMatrix.value[0] = inverse00;
	outMatrix.value[1] = inverse10;
	outMatrix.value[2] = inverse20;
	outMatrix.value[3] = inverse01;
	outMatrix.value[4] = inverse11;
	outMatrix.value[5] = inverse21;
	outMatrix.value[6] = inverse02;
	outMatrix.value[7] = inverse12;
	outMatrix.value[8] = inverse22;
	return true;
}

static void buildFallbackNormalMatrix(const float4x4& matrix, float3x3& outMatrix)
{
	outMatrix.value[0] = matrix.value[0];
	outMatrix.value[1] = matrix.value[1];
	outMatrix.value[2] = matrix.value[2];
	outMatrix.value[3] = matrix.value[4];
	outMatrix.value[4] = matrix.value[5];
	outMatrix.value[5] = matrix.value[6];
	outMatrix.value[6] = matrix.value[8];
	outMatrix.value[7] = matrix.value[9];
	outMatrix.value[8] = matrix.value[10];
}

static float3 transformFbxPosition(const float3& position, const float4x4& worldMatrix)
{
	float3 transformedPosition = {};
	transformedPosition.x = position.x * worldMatrix.value[0]
		+ position.y * worldMatrix.value[4]
		+ position.z * worldMatrix.value[8]
		+ worldMatrix.value[12];
	transformedPosition.y = position.x * worldMatrix.value[1]
		+ position.y * worldMatrix.value[5]
		+ position.z * worldMatrix.value[9]
		+ worldMatrix.value[13];
	transformedPosition.z = position.x * worldMatrix.value[2]
		+ position.y * worldMatrix.value[6]
		+ position.z * worldMatrix.value[10]
		+ worldMatrix.value[14];
	return transformedPosition;
}

static float3 transformFbxVector(const float3& value, const float3x3& matrix)
{
	float3 transformedValue = {};
	transformedValue.x = value.x * matrix.value[0]
		+ value.y * matrix.value[3]
		+ value.z * matrix.value[6];
	transformedValue.y = value.x * matrix.value[1]
		+ value.y * matrix.value[4]
		+ value.z * matrix.value[7];
	transformedValue.z = value.x * matrix.value[2]
		+ value.y * matrix.value[5]
		+ value.z * matrix.value[8];
	return transformedValue;
}

static float3 normalizeFbxVector(const float3& value)
{
	const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
	if (lengthSquared <= 0.000001f)
	{
		return {};
	}

	const float inverseLength = 1.0f / sqrtf(lengthSquared);
	float3 normalizedValue = {};
	normalizedValue.x = value.x * inverseLength;
	normalizedValue.y = value.y * inverseLength;
	normalizedValue.z = value.z * inverseLength;
	return normalizedValue;
}

static bool resolveFbxMappedValueIndex(
	const string& mappingInformationType,
	const string& referenceInformationType,
	const uint32 polygonVertexIndex,
	const uint32 controlPointIndex,
	const uint32 directValueCount,
	const vector<int32>& directIndices,
	uint32& outDirectValueIndex)
{
	uint32 mappedValueIndex = 0;
	if (mappingInformationType == "ByPolygonVertex")
	{
		mappedValueIndex = polygonVertexIndex;
	}
	else if (mappingInformationType == "ByVertice" || mappingInformationType == "ByVertex" || mappingInformationType == "ByControlPoint")
	{
		mappedValueIndex = controlPointIndex;
	}
	else if (mappingInformationType == "AllSame")
	{
		mappedValueIndex = 0;
	}
	else
	{
		return false;
	}

	if (referenceInformationType == "Direct")
	{
		outDirectValueIndex = mappedValueIndex;
	}
	else if (referenceInformationType == "IndexToDirect")
	{
		if (mappedValueIndex >= static_cast<uint32>(directIndices.size()) || directIndices[mappedValueIndex] < 0)
		{
			return false;
		}

		outDirectValueIndex = static_cast<uint32>(directIndices[mappedValueIndex]);
	}
	else
	{
		return false;
	}

	return outDirectValueIndex < directValueCount;
}

static const FbxLayerElementFloat2* selectPrimaryFbxTexcoordLayer(const FbxGeometryData& geometryData)
{
	for (uint32 uvLayerIndex = 0; uvLayerIndex < static_cast<uint32>(geometryData.uvLayers.size()); ++uvLayerIndex)
	{
		const FbxLayerElementFloat2& uvLayer = geometryData.uvLayers[uvLayerIndex];
		if (uvLayer.name == "TextureUV")
		{
			return &uvLayer;
		}
	}

	if (geometryData.uvLayers.empty())
	{
		return nullptr;
	}

	return &geometryData.uvLayers[0];
}

static uint32 countFbxGeometryPolygons(const FbxGeometryData& geometryData)
{
	uint32 polygonCount = 0;
	for (uint32 polygonVertexArrayIndex = 0; polygonVertexArrayIndex < static_cast<uint32>(geometryData.polygonVertexIndices.size()); ++polygonVertexArrayIndex)
	{
		if (geometryData.polygonVertexIndices[polygonVertexArrayIndex] < 0)
		{
			++polygonCount;
		}
	}

	return polygonCount;
}

static bool resolveFbxMaterialSlotIndex(
	const FbxLayerElementMaterial& materials,
	const uint32 polygonIndex,
	int32& outMaterialSlotIndex)
{
	outMaterialSlotIndex = 0;
	if (materials.materialIndices.empty())
	{
		return true;
	}

	const string& referenceInformationType = materials.referenceInformationType;
	if (!referenceInformationType.empty()
		&& referenceInformationType != "IndexToDirect"
		&& referenceInformationType != "Direct")
	{
		return false;
	}

	const string& mappingInformationType = materials.mappingInformationType;
	if (mappingInformationType == "AllSame")
	{
		outMaterialSlotIndex = materials.materialIndices[0];
		return true;
	}

	if (mappingInformationType == "ByPolygon")
	{
		if (polygonIndex >= static_cast<uint32>(materials.materialIndices.size()))
		{
			return false;
		}

		outMaterialSlotIndex = materials.materialIndices[polygonIndex];
		return true;
	}

	return false;
}

static bool buildFbxImportSections(
	const FbxGeometryData& geometryData,
	const vector<FbxObjectIdentifier>& materialIdentifiers,
	vector<FbxImportSectionSource>& outSections)
{
	outSections.clear();

	const uint32 polygonCount = countFbxGeometryPolygons(geometryData);
	if (polygonCount == 0)
	{
		return true;
	}

	int32 activeMaterialSlotIndex = -2;
	FbxObjectIdentifier activeMaterialIdentifier = 0;
	uint32 activePolygonStartIndex = 0;
	uint32 activePolygonCount = 0;
	for (uint32 polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex)
	{
		int32 materialSlotIndex = -1;
		if (!resolveFbxMaterialSlotIndex(geometryData.materials, polygonIndex, materialSlotIndex))
		{
			return false;
		}

		const FbxObjectIdentifier materialIdentifier =
			materialSlotIndex >= 0 && static_cast<uint32>(materialSlotIndex) < materialIdentifiers.size()
				? materialIdentifiers[static_cast<uint32>(materialSlotIndex)]
				: 0;
		const bool continueActiveSection = materialSlotIndex == activeMaterialSlotIndex
			&& materialIdentifier == activeMaterialIdentifier;
		if (continueActiveSection)
		{
			++activePolygonCount;
			continue;
		}

		if (activePolygonCount != 0)
		{
			outSections.push_back({
				.polygonStartIndex = activePolygonStartIndex,
				.polygonCount = activePolygonCount,
				.materialSlotIndex = activeMaterialSlotIndex,
				.materialIdentifier = activeMaterialIdentifier,
			});
		}

		activePolygonStartIndex = polygonIndex;
		activePolygonCount = 1;
		activeMaterialSlotIndex = materialSlotIndex;
		activeMaterialIdentifier = materialIdentifier;
	}

	if (activePolygonCount != 0)
	{
		outSections.push_back({
			.polygonStartIndex = activePolygonStartIndex,
			.polygonCount = activePolygonCount,
			.materialSlotIndex = activeMaterialSlotIndex,
			.materialIdentifier = activeMaterialIdentifier,
		});
	}

	return !outSections.empty();
}

static bool appendFbxGeometryPolygonRangeToRawMeshData(
	const FbxGeometryData& geometryData,
	const float4x4& worldMatrix,
	const float3x3& normalMatrix,
	const uint32 polygonStartIndex,
	const uint32 polygonCount,
	RawMeshData& inOutRawMeshData)
{
	const uint32 controlPointCount = static_cast<uint32>(geometryData.vertices.size() / 3);
	const FbxLayerElementFloat2* primaryTexcoordLayer = selectPrimaryFbxTexcoordLayer(geometryData);
	const uint32 normalDirectValueCount = static_cast<uint32>(geometryData.normals.directValues.size() / 3);
	const uint32 texcoordDirectValueCount = primaryTexcoordLayer != nullptr
		? static_cast<uint32>(primaryTexcoordLayer->directValues.size() / 2)
		: 0;
	const uint32 polygonEndIndex = polygonStartIndex + polygonCount;

	unordered_map<FbxVertexKey, uint32, FbxVertexKeyHasher> localVertexMap = {};
	localVertexMap.reserve(geometryData.polygonVertexIndices.size());

	vector<uint32> faceIndices = {};
	faceIndices.reserve(4);
	uint32 polygonVertexIndex = 0;
	uint32 currentPolygonIndex = 0;
	for (uint32 polygonVertexArrayIndex = 0; polygonVertexArrayIndex < static_cast<uint32>(geometryData.polygonVertexIndices.size()); ++polygonVertexArrayIndex)
	{
		const int32 polygonVertexControlPointValue = geometryData.polygonVertexIndices[polygonVertexArrayIndex];
		const bool polygonEnds = polygonVertexControlPointValue < 0;
		const uint32 controlPointIndex = static_cast<uint32>(polygonEnds
			? (-polygonVertexControlPointValue) - 1
			: polygonVertexControlPointValue);
		if (controlPointIndex >= controlPointCount)
		{
			return false;
		}

		const bool polygonSelected = currentPolygonIndex >= polygonStartIndex && currentPolygonIndex < polygonEndIndex;

		uint32 normalIndex = uint32MaxValue;
		if (polygonSelected && !geometryData.normals.directValues.empty())
		{
			if (!resolveFbxMappedValueIndex(
				geometryData.normals.mappingInformationType,
				geometryData.normals.referenceInformationType.empty() ? "Direct" : geometryData.normals.referenceInformationType,
				polygonVertexIndex,
				controlPointIndex,
				normalDirectValueCount,
				geometryData.normals.indices,
				normalIndex))
			{
				return false;
			}
		}

		uint32 texcoordIndex = uint32MaxValue;
		if (polygonSelected && primaryTexcoordLayer != nullptr && !primaryTexcoordLayer->directValues.empty())
		{
			if (!resolveFbxMappedValueIndex(
				primaryTexcoordLayer->mappingInformationType,
				primaryTexcoordLayer->referenceInformationType,
				polygonVertexIndex,
				controlPointIndex,
				texcoordDirectValueCount,
				primaryTexcoordLayer->indices,
				texcoordIndex))
			{
				return false;
			}
		}

		if (polygonSelected)
		{
			const FbxVertexKey vertexKey = { controlPointIndex, normalIndex, texcoordIndex };
			auto foundVertex = localVertexMap.find(vertexKey);
			if (foundVertex == localVertexMap.end())
			{
				PositionData positionVertex = {};
				positionVertex.x = static_cast<float>(geometryData.vertices[static_cast<size_t>(controlPointIndex) * 3 + 0]);
				positionVertex.y = static_cast<float>(geometryData.vertices[static_cast<size_t>(controlPointIndex) * 3 + 1]);
				positionVertex.z = static_cast<float>(geometryData.vertices[static_cast<size_t>(controlPointIndex) * 3 + 2]);
				positionVertex = transformFbxPosition(positionVertex, worldMatrix);

				NormalData normalVertex = {};
				if (normalIndex != uint32MaxValue)
				{
					normalVertex.x = static_cast<float>(geometryData.normals.directValues[static_cast<size_t>(normalIndex) * 3 + 0]);
					normalVertex.y = static_cast<float>(geometryData.normals.directValues[static_cast<size_t>(normalIndex) * 3 + 1]);
					normalVertex.z = static_cast<float>(geometryData.normals.directValues[static_cast<size_t>(normalIndex) * 3 + 2]);
					normalVertex = normalizeFbxVector(transformFbxVector(normalVertex, normalMatrix));
				}

				TexcoordData texcoordVertex = {};
				if (primaryTexcoordLayer != nullptr && texcoordIndex != uint32MaxValue)
				{
					texcoordVertex.x = static_cast<float>(primaryTexcoordLayer->directValues[static_cast<size_t>(texcoordIndex) * 2 + 0]);
					texcoordVertex.y = static_cast<float>(primaryTexcoordLayer->directValues[static_cast<size_t>(texcoordIndex) * 2 + 1]);
				}

				const uint32 newVertexIndex = static_cast<uint32>(inOutRawMeshData.positionVertices.size());
				inOutRawMeshData.positionVertices.push_back(positionVertex);
				inOutRawMeshData.normalVertices.push_back(normalVertex);
				inOutRawMeshData.texcoordVertices.push_back(texcoordVertex);
				localVertexMap.emplace(vertexKey, newVertexIndex);
				faceIndices.push_back(newVertexIndex);
			}
			else
			{
				faceIndices.push_back(foundVertex->second);
			}
		}

		++polygonVertexIndex;
		if (!polygonEnds)
		{
			continue;
		}

		if (polygonSelected && faceIndices.size() < 3)
		{
			return false;
		}

		if (polygonSelected)
		{
			for (uint32 triangleIndex = 1; triangleIndex + 1 < static_cast<uint32>(faceIndices.size()); ++triangleIndex)
			{
				inOutRawMeshData.indices.push_back(faceIndices[0]);
				inOutRawMeshData.indices.push_back(faceIndices[triangleIndex]);
				inOutRawMeshData.indices.push_back(faceIndices[triangleIndex + 1]);
			}
		}

		faceIndices.clear();
		++currentPolygonIndex;
	}

	return faceIndices.empty() && currentPolygonIndex == countFbxGeometryPolygons(geometryData);
}

static bool appendFbxMeshSectionsToRawMeshData(
	const string& meshFilePath,
	const FbxGeometryData& geometryData,
	const vector<FbxImportSectionSource>& sections,
	const float4x4& worldMatrix,
	const bool skipFailedSections,
	RawMeshData& inOutRawMeshData,
	string& outErrorText)
{
	float3x3 normalMatrix = {};
	if (!buildInverseTranspose3x3(worldMatrix, normalMatrix))
	{
		buildFallbackNormalMatrix(worldMatrix, normalMatrix);
	}

	for (uint32 sectionIndex = 0; sectionIndex < static_cast<uint32>(sections.size()); ++sectionIndex)
	{
		const FbxImportSectionSource& section = sections[sectionIndex];
		const uint32 sectionStartIndex = static_cast<uint32>(inOutRawMeshData.indices.size());
		if (!appendFbxGeometryPolygonRangeToRawMeshData(
			geometryData,
			worldMatrix,
			normalMatrix,
			section.polygonStartIndex,
			section.polygonCount,
			inOutRawMeshData))
		{
			if (!skipFailedSections)
			{
				return failFbxMeshBuild(meshFilePath, "fbx_geometry_append_failed", outErrorText);
			}

			error << "[FbxMeshParserStub][Warning] path=" << meshFilePath
				  << " geometry=" << geometryData.name
				  << " reason=fbx_geometry_append_failed"
				  << " polygonStart=" << section.polygonStartIndex
				  << " polygonCount=" << section.polygonCount
				  << lineBreak;
			continue;
		}

		const uint32 sectionIndexCount = static_cast<uint32>(inOutRawMeshData.indices.size()) - sectionStartIndex;
		if (sectionIndexCount == 0)
		{
			continue;
		}

		uint16 materialSlotIndex = RawMeshData::invalidMaterialSlotIndex;
		if (section.materialSlotIndex >= 0)
		{
			assert(static_cast<uint32>(section.materialSlotIndex) < static_cast<uint32>(RawMeshData::invalidMaterialSlotIndex) && "[FbxMeshParserStub][Assert] reason=material_slot_index_out_of_range");
			materialSlotIndex = static_cast<uint16>(section.materialSlotIndex);
		}

		inOutRawMeshData.sectionRanges.push_back({
			.startIndex = sectionStartIndex,
			.indexCount = sectionIndexCount,
		});
		inOutRawMeshData.sectionMaterialSlotIndices.push_back(materialSlotIndex);
	}

	return true;
}

static bool buildFbxImportMeshNodes(
	const FbxSceneData& sceneData,
	vector<FbxImportMeshNode>& outMeshNodes,
	string& outErrorText)
{
	outErrorText.clear();
	outMeshNodes.clear();

	unordered_map<FbxObjectIdentifier, float4x4> modelWorldMatrixByIdentifier = {};
	if (!buildFbxModelWorldMatrices(sceneData, modelWorldMatrixByIdentifier))
	{
		outErrorText = "fbx_model_world_matrix_build_failed";
		return false;
	}

	for (auto modelIterator = sceneData.modelToGeometryIdentifiers.begin();
		modelIterator != sceneData.modelToGeometryIdentifiers.end();
		++modelIterator)
	{
		const FbxObjectIdentifier modelIdentifier = modelIterator->first;
		auto foundModel = sceneData.modelByIdentifier.find(modelIdentifier);
		if (foundModel == sceneData.modelByIdentifier.end())
		{
			continue;
		}

		const auto foundWorldMatrix = modelWorldMatrixByIdentifier.find(modelIdentifier);
		if (foundWorldMatrix == modelWorldMatrixByIdentifier.end())
		{
			continue;
		}

		const float4x4 worldMatrix = foundWorldMatrix->second;
		const auto foundModelMaterials = sceneData.modelToMaterialIdentifiers.find(modelIdentifier);
		const vector<FbxObjectIdentifier>& modelMaterialIdentifiers =
			foundModelMaterials != sceneData.modelToMaterialIdentifiers.end()
				? foundModelMaterials->second
				: vector<FbxObjectIdentifier>();
		for (uint32 geometryIndex = 0; geometryIndex < static_cast<uint32>(modelIterator->second.size()); ++geometryIndex)
		{
			const FbxObjectIdentifier geometryIdentifier = modelIterator->second[geometryIndex];
			auto foundGeometry = sceneData.geometryByIdentifier.find(geometryIdentifier);
			if (foundGeometry == sceneData.geometryByIdentifier.end())
			{
				continue;
			}

			FbxImportMeshNode meshNode = {
				.geometryData = &foundGeometry->second,
				.geometryIdentifier = geometryIdentifier,
				.geometryName = foundGeometry->second.name,
				.modelIdentifier = modelIdentifier,
				.modelName = foundModel->second.name,
				.worldMatrix = worldMatrix,
				.materialIdentifiers = modelMaterialIdentifiers,
			};
			if (!buildFbxImportSections(foundGeometry->second, meshNode.materialIdentifiers, meshNode.sections))
			{
				outErrorText = "fbx_material_section_mapping_invalid";
				return false;
			}

			outMeshNodes.push_back(moveValue(meshNode));
		}
	}

	return true;
}

bool FbxMeshParserStub::parse(
	const string& meshFilePath,
	const uint32 lodLevel,
	MeshAsset& outMeshAsset,
	string& outErrorText) const
{
	outMeshAsset = {};
	outErrorText.clear();
	RawMeshData& rawMeshData = outMeshAsset.getRawMeshData(lodLevel);
	rawMeshData.empty();

	FbxSceneData sceneData = {};
	if (!parseFbxSceneData(meshFilePath, sceneData, outErrorText))
	{
		return false;
	}

	vector<FbxImportMeshNode> meshNodes = {};
	if (!buildFbxImportMeshNodes(sceneData, meshNodes, outErrorText))
	{
		return false;
	}

	for (uint32 meshNodeIndex = 0; meshNodeIndex < static_cast<uint32>(meshNodes.size()); ++meshNodeIndex)
	{
		const FbxImportMeshNode& meshNode = meshNodes[meshNodeIndex];
		if (meshNode.geometryData == nullptr)
		{
			continue;
		}

		if (!appendFbxMeshSectionsToRawMeshData(
			meshFilePath,
			*meshNode.geometryData,
			meshNode.sections,
			meshNode.worldMatrix,
			true,
			rawMeshData,
			outErrorText))
		{
			return false;
		}
	}

	if (rawMeshData.positionVertices.empty() || rawMeshData.indices.empty())
	{
		return failFbxMeshBuild(meshFilePath, "fbx_mesh_data_empty", outErrorText);
	}

	if (!rawMeshData.isValid())
	{
		return failFbxMeshBuild(meshFilePath, "fbx_mesh_vertex_stream_mismatch", outErrorText);
	}

	outMeshAsset.setName(meshFilePath + ":LOD" + to_string(lodLevel));
	outMeshAsset.setSource(meshFilePath);
	return true;
}

bool FbxMeshParserStub::importEntityHierarchy(
	const string& meshFilePath,
	const string& meshAssetDirectoryPath,
	World& outWorld,
	const uint32 parentEntityIndex,
	string& outErrorText) const
{
	outErrorText.clear();
	assert(!meshAssetDirectoryPath.empty() && "[FbxMeshParserStub][Assert] reason=mesh_asset_directory_path_missing");

	FbxSceneData sceneData = {};
	if (!parseFbxSceneData(meshFilePath, sceneData, outErrorText))
	{
		return false;
	}

	unordered_map<FbxObjectIdentifier, bool> relevantModelIdentifiers = {};
	for (auto modelIterator = sceneData.modelToGeometryIdentifiers.begin();
		modelIterator != sceneData.modelToGeometryIdentifiers.end();
		++modelIterator)
	{
		FbxObjectIdentifier traversalModelIdentifier = modelIterator->first;
		while (traversalModelIdentifier != 0)
		{
			if (relevantModelIdentifiers.find(traversalModelIdentifier) != relevantModelIdentifiers.end())
			{
				break;
			}

			relevantModelIdentifiers.emplace(traversalModelIdentifier, true);
			const auto foundModel = sceneData.modelByIdentifier.find(traversalModelIdentifier);
			if (foundModel == sceneData.modelByIdentifier.end())
			{
				break;
			}

			traversalModelIdentifier = foundModel->second.parentModelIdentifier;
		}
	}

	if (relevantModelIdentifiers.empty())
	{
		return failFbxMeshBuild(meshFilePath, "fbx_scene_no_mesh_models", outErrorText);
	}

	const Entity* parentEntity = outWorld.getEntityByIndex(parentEntityIndex);
	assert(parentEntity != nullptr && "[FbxMeshParserStub][Assert] reason=parent_entity_missing");
	if (parentEntity == nullptr)
	{
		return failFbxMeshBuild(meshFilePath, "fbx_scene_parent_entity_missing", outErrorText);
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	assert(diskLoaderModule != nullptr && "[FbxMeshParserStub][Assert] reason=disk_loader_module_missing");

	unordered_map<FbxObjectIdentifier, bool> uniqueGeometryIdentifiers = {};
	vector<FbxObjectIdentifier> geometryIdentifiers = {};
	for (auto modelIterator = sceneData.modelToGeometryIdentifiers.begin();
		modelIterator != sceneData.modelToGeometryIdentifiers.end();
		++modelIterator)
	{
		if (relevantModelIdentifiers.find(modelIterator->first) == relevantModelIdentifiers.end())
		{
			continue;
		}

		for (uint32 geometryIndex = 0; geometryIndex < static_cast<uint32>(modelIterator->second.size()); ++geometryIndex)
		{
			const FbxObjectIdentifier geometryIdentifier = modelIterator->second[geometryIndex];
			if (uniqueGeometryIdentifiers.find(geometryIdentifier) != uniqueGeometryIdentifiers.end())
			{
				continue;
			}

			uniqueGeometryIdentifiers.emplace(geometryIdentifier, true);
			geometryIdentifiers.push_back(geometryIdentifier);
		}
	}

	sort(
		geometryIdentifiers.begin(),
		geometryIdentifiers.end(),
		[&sceneData](const FbxObjectIdentifier left, const FbxObjectIdentifier right)
		{
			const auto foundLeftGeometry = sceneData.geometryByIdentifier.find(left);
			const auto foundRightGeometry = sceneData.geometryByIdentifier.find(right);
			const string& leftName = foundLeftGeometry != sceneData.geometryByIdentifier.end() ? foundLeftGeometry->second.name : string();
			const string& rightName = foundRightGeometry != sceneData.geometryByIdentifier.end() ? foundRightGeometry->second.name : string();
			return leftName == rightName ? left < right : leftName < rightName;
		});

	unordered_map<FbxObjectIdentifier, string> geometryAssetPathByIdentifier = {};
	for (uint32 geometryArrayIndex = 0; geometryArrayIndex < static_cast<uint32>(geometryIdentifiers.size()); ++geometryArrayIndex)
	{
		const FbxObjectIdentifier geometryIdentifier = geometryIdentifiers[geometryArrayIndex];
		const auto foundGeometry = sceneData.geometryByIdentifier.find(geometryIdentifier);
		if (foundGeometry == sceneData.geometryByIdentifier.end())
		{
			return failFbxMeshBuild(meshFilePath, "fbx_geometry_missing", outErrorText);
		}

		vector<FbxImportSectionSource> sections = {};
		const vector<FbxObjectIdentifier> materialIdentifiers = {};
		if (!buildFbxImportSections(foundGeometry->second, materialIdentifiers, sections))
		{
			return failFbxMeshBuild(meshFilePath, "fbx_material_section_mapping_invalid", outErrorText);
		}

		MeshAsset meshAsset = {};
		meshAsset.ensureLODCount(1);
		RawMeshData& rawMeshData = meshAsset.getRawMeshData(0);
		rawMeshData.empty();
		if (!appendFbxMeshSectionsToRawMeshData(
			meshFilePath,
			foundGeometry->second,
			sections,
			buildIdentityMatrix4x4(),
			true,
			rawMeshData,
			outErrorText))
		{
			return false;
		}

		if (rawMeshData.positionVertices.empty() || rawMeshData.indices.empty())
		{
			error << "[FbxMeshParserStub][Warning] path=" << meshFilePath
				  << " geometry=" << foundGeometry->second.name
				  << " reason=fbx_geometry_mesh_data_empty"
				  << lineBreak;
			continue;
		}

		if (!rawMeshData.isValid())
		{
			error << "[FbxMeshParserStub][Warning] path=" << meshFilePath
				  << " geometry=" << foundGeometry->second.name
				  << " reason=fbx_mesh_vertex_stream_mismatch"
				  << lineBreak;
			continue;
		}

		const string geometryName = !foundGeometry->second.name.empty()
			? foundGeometry->second.name
			: "Geometry_" + to_string(geometryIdentifier);
		const string geometryFileStem = diskLoaderModule->sanitizeFileName(geometryName, "Geometry");
		const string meshAssetPath = (filesystem_path(meshAssetDirectoryPath) / (geometryFileStem + "_" + to_string(geometryIdentifier) + ".deasset"))
			.lexically_normal()
			.generic_string();
		meshAsset.setName(geometryName);
		meshAsset.setSource(meshFilePath);
		meshAsset.setAssetPath(meshAssetPath);

		OutputFileStream meshAssetFileStream =
			diskLoaderModule->openOutputFileStream(diskLoaderModule->resolveAbsolutePathFromResources(meshAssetPath), false, true);
		meshAsset.writeProperty(meshAssetFileStream);
		geometryAssetPathByIdentifier.emplace(geometryIdentifier, meshAssetPath);
	}

	if (geometryAssetPathByIdentifier.empty())
	{
		return failFbxMeshBuild(meshFilePath, "fbx_scene_geometry_import_empty", outErrorText);
	}

	unordered_map<FbxObjectIdentifier, vector<FbxObjectIdentifier>> childModelIdentifiers = {};
	vector<FbxObjectIdentifier> rootModelIdentifiers = {};
	for (auto relevantModelIterator = relevantModelIdentifiers.begin();
		relevantModelIterator != relevantModelIdentifiers.end();
		++relevantModelIterator)
	{
		const auto foundModel = sceneData.modelByIdentifier.find(relevantModelIterator->first);
		if (foundModel == sceneData.modelByIdentifier.end())
		{
			continue;
		}

		const FbxObjectIdentifier parentModelIdentifier = foundModel->second.parentModelIdentifier;
		if (parentModelIdentifier != 0 && relevantModelIdentifiers.find(parentModelIdentifier) != relevantModelIdentifiers.end())
		{
			childModelIdentifiers[parentModelIdentifier].push_back(relevantModelIterator->first);
			continue;
		}

		rootModelIdentifiers.push_back(relevantModelIterator->first);
	}

	for (auto childModelIterator = childModelIdentifiers.begin();
		childModelIterator != childModelIdentifiers.end();
		++childModelIterator)
	{
		vector<FbxObjectIdentifier>& childIdentifiers = childModelIterator->second;
		sort(
			childIdentifiers.begin(),
			childIdentifiers.end(),
			[&sceneData](const FbxObjectIdentifier left, const FbxObjectIdentifier right)
			{
				const auto foundLeftModel = sceneData.modelByIdentifier.find(left);
				const auto foundRightModel = sceneData.modelByIdentifier.find(right);
				const string& leftName = foundLeftModel != sceneData.modelByIdentifier.end() ? foundLeftModel->second.name : string();
				const string& rightName = foundRightModel != sceneData.modelByIdentifier.end() ? foundRightModel->second.name : string();
				return leftName == rightName ? left < right : leftName < rightName;
			});
	}

	sort(
		rootModelIdentifiers.begin(),
		rootModelIdentifiers.end(),
		[&sceneData](const FbxObjectIdentifier left, const FbxObjectIdentifier right)
		{
			const auto foundLeftModel = sceneData.modelByIdentifier.find(left);
			const auto foundRightModel = sceneData.modelByIdentifier.find(right);
			const string& leftName = foundLeftModel != sceneData.modelByIdentifier.end() ? foundLeftModel->second.name : string();
			const string& rightName = foundRightModel != sceneData.modelByIdentifier.end() ? foundRightModel->second.name : string();
			return leftName == rightName ? left < right : leftName < rightName;
		});

	auto importModelEntity = [&](const auto& recursiveImport, const FbxObjectIdentifier modelIdentifier, const uint32 destinationParentEntityIndex) -> bool
	{
		const auto foundModel = sceneData.modelByIdentifier.find(modelIdentifier);
		if (foundModel == sceneData.modelByIdentifier.end())
		{
			return false;
		}

		const uint32 modelEntityIndex = outWorld.createPlaceableEntity();
		PlaceableEntity* modelEntity = dynamic_cast<PlaceableEntity*>(outWorld.getEntityByIndex(modelEntityIndex));
		assert(modelEntity != nullptr && "[FbxMeshParserStub][Assert] reason=model_entity_create_failed");
		if (modelEntity == nullptr)
		{
			return false;
		}

		const string modelName = !foundModel->second.name.empty()
			? foundModel->second.name
			: "Model_" + to_string(modelIdentifier);
		modelEntity->setName(modelName);
		// TODO: Replace this TEMP_ local-transform import path once the final runtime hierarchy transform flow is settled.
		modelEntity->transform.positionX = foundModel->second.translation.x;
		modelEntity->transform.positionY = foundModel->second.translation.y;
		modelEntity->transform.positionZ = foundModel->second.translation.z;
		modelEntity->transform.rotationPitch = toFbxRadians(foundModel->second.rotationInDegrees.x);
		modelEntity->transform.rotationYaw = toFbxRadians(foundModel->second.rotationInDegrees.y);
		modelEntity->transform.rotationRoll = toFbxRadians(foundModel->second.rotationInDegrees.z);
		modelEntity->transform.scaleX = foundModel->second.scaling.x;
		modelEntity->transform.scaleY = foundModel->second.scaling.y;
		modelEntity->transform.scaleZ = foundModel->second.scaling.z;

		const bool addedModelEntity = outWorld.addChildEntity(destinationParentEntityIndex, modelEntityIndex);
		assert(addedModelEntity && "[FbxMeshParserStub][Assert] reason=model_entity_attach_failed");
		if (!addedModelEntity)
		{
			return false;
		}

		auto foundModelGeometries = sceneData.modelToGeometryIdentifiers.find(modelIdentifier);
		if (foundModelGeometries != sceneData.modelToGeometryIdentifiers.end() && !foundModelGeometries->second.empty())
		{
			vector<FbxObjectIdentifier> modelGeometryIdentifiers = foundModelGeometries->second;
			sort(
				modelGeometryIdentifiers.begin(),
				modelGeometryIdentifiers.end(),
				[&sceneData](const FbxObjectIdentifier left, const FbxObjectIdentifier right)
				{
					const auto foundLeftGeometry = sceneData.geometryByIdentifier.find(left);
					const auto foundRightGeometry = sceneData.geometryByIdentifier.find(right);
					const string& leftName = foundLeftGeometry != sceneData.geometryByIdentifier.end() ? foundLeftGeometry->second.name : string();
					const string& rightName = foundRightGeometry != sceneData.geometryByIdentifier.end() ? foundRightGeometry->second.name : string();
					return leftName == rightName ? left < right : leftName < rightName;
				});

			if (modelGeometryIdentifiers.size() == 1)
			{
				const auto foundMeshAssetPath = geometryAssetPathByIdentifier.find(modelGeometryIdentifiers[0]);
				if (foundMeshAssetPath == geometryAssetPathByIdentifier.end())
				{
					error << "[FbxMeshParserStub][Warning] path=" << meshFilePath
						  << " model=" << modelName
						  << " reason=model_geometry_mesh_asset_missing"
						  << lineBreak;
				}
				else
				{
					unique_pointer<MeshComponent> meshComponent(new MeshComponent());
					meshComponent->setMeshAssetPath(foundMeshAssetPath->second);
					const bool attachedMeshComponent = outWorld.attachComponent(modelEntityIndex, moveValue(meshComponent));
					assert(attachedMeshComponent && "[FbxMeshParserStub][Assert] reason=model_mesh_component_attach_failed");
					if (!attachedMeshComponent)
					{
						return false;
					}
				}
			}
			else
			{
				for (uint32 geometryArrayIndex = 0; geometryArrayIndex < static_cast<uint32>(modelGeometryIdentifiers.size()); ++geometryArrayIndex)
				{
					const FbxObjectIdentifier geometryIdentifier = modelGeometryIdentifiers[geometryArrayIndex];
					const auto foundGeometry = sceneData.geometryByIdentifier.find(geometryIdentifier);
					const auto foundMeshAssetPath = geometryAssetPathByIdentifier.find(geometryIdentifier);
					if (foundGeometry == sceneData.geometryByIdentifier.end() || foundMeshAssetPath == geometryAssetPathByIdentifier.end())
					{
						error << "[FbxMeshParserStub][Warning] path=" << meshFilePath
							  << " model=" << modelName
							  << " reason=model_geometry_mesh_asset_missing"
							  << lineBreak;
						continue;
					}

					const uint32 meshEntityIndex = outWorld.createPlaceableEntity();
					PlaceableEntity* meshEntity = dynamic_cast<PlaceableEntity*>(outWorld.getEntityByIndex(meshEntityIndex));
					assert(meshEntity != nullptr && "[FbxMeshParserStub][Assert] reason=model_geometry_entity_create_failed");
					if (meshEntity == nullptr)
					{
						return false;
					}

					meshEntity->setName(!foundGeometry->second.name.empty()
						? foundGeometry->second.name
						: "Geometry_" + to_string(geometryIdentifier));
					// TODO: Replace this TEMP_ identity child transform once the final runtime hierarchy transform flow is settled.
					meshEntity->transform = {};

					const bool addedMeshEntity = outWorld.addChildEntity(modelEntityIndex, meshEntityIndex);
					assert(addedMeshEntity && "[FbxMeshParserStub][Assert] reason=model_geometry_entity_attach_failed");
					if (!addedMeshEntity)
					{
						return false;
					}

					unique_pointer<MeshComponent> meshComponent(new MeshComponent());
					meshComponent->setMeshAssetPath(foundMeshAssetPath->second);
					const bool attachedMeshComponent = outWorld.attachComponent(meshEntityIndex, moveValue(meshComponent));
					assert(attachedMeshComponent && "[FbxMeshParserStub][Assert] reason=model_geometry_mesh_component_attach_failed");
					if (!attachedMeshComponent)
					{
						return false;
					}
				}
			}
		}
		const auto foundChildModels = childModelIdentifiers.find(modelIdentifier);
		if (foundChildModels == childModelIdentifiers.end())
		{
			return true;
		}

		for (uint32 childIndex = 0; childIndex < static_cast<uint32>(foundChildModels->second.size()); ++childIndex)
		{
			if (!recursiveImport(recursiveImport, foundChildModels->second[childIndex], modelEntityIndex))
			{
				return false;
			}
		}

		return true;
	};

	for (uint32 rootModelIndex = 0; rootModelIndex < static_cast<uint32>(rootModelIdentifiers.size()); ++rootModelIndex)
	{
		if (!importModelEntity(importModelEntity, rootModelIdentifiers[rootModelIndex], parentEntityIndex))
		{
			if (outErrorText.empty())
			{
				outErrorText = "fbx_scene_entity_import_failed";
			}

			return failFbxMeshBuild(meshFilePath, outErrorText.empty() ? "fbx_scene_entity_import_failed" : outErrorText, outErrorText);
		}
	}

	return true;
}
