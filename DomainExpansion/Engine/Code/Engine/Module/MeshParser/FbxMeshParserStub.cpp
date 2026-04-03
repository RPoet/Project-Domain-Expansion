#include "Engine/Module/MeshParser/FbxMeshParserStub.h"

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

static bool appendFbxGeometryToRawMeshData(
	const FbxGeometryData& geometryData,
	const float4x4& worldMatrix,
	const float3x3& normalMatrix,
	RawMeshData& inOutRawMeshData)
{
	const uint32 controlPointCount = static_cast<uint32>(geometryData.vertices.size() / 3);
	const FbxLayerElementFloat2* primaryTexcoordLayer = selectPrimaryFbxTexcoordLayer(geometryData);
	const uint32 normalDirectValueCount = static_cast<uint32>(geometryData.normals.directValues.size() / 3);
	const uint32 texcoordDirectValueCount = primaryTexcoordLayer != nullptr
		? static_cast<uint32>(primaryTexcoordLayer->directValues.size() / 2)
		: 0;

	unordered_map<FbxVertexKey, uint32, FbxVertexKeyHasher> localVertexMap = {};
	localVertexMap.reserve(geometryData.polygonVertexIndices.size());

	vector<uint32> faceIndices = {};
	faceIndices.reserve(4);
	uint32 polygonVertexIndex = 0;
	for (uint32 polygonIndex = 0; polygonIndex < static_cast<uint32>(geometryData.polygonVertexIndices.size()); ++polygonIndex)
	{
		const int32 polygonVertexControlPointValue = geometryData.polygonVertexIndices[polygonIndex];
		const bool polygonEnds = polygonVertexControlPointValue < 0;
		const uint32 controlPointIndex = static_cast<uint32>(polygonEnds
			? (-polygonVertexControlPointValue) - 1
			: polygonVertexControlPointValue);
		if (controlPointIndex >= controlPointCount)
		{
			return false;
		}

		uint32 normalIndex = uint32MaxValue;
		if (!geometryData.normals.directValues.empty())
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
		if (primaryTexcoordLayer != nullptr && !primaryTexcoordLayer->directValues.empty())
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

		++polygonVertexIndex;
		if (!polygonEnds)
		{
			continue;
		}

		if (faceIndices.size() < 3)
		{
			return false;
		}

		for (uint32 triangleIndex = 1; triangleIndex + 1 < static_cast<uint32>(faceIndices.size()); ++triangleIndex)
		{
			inOutRawMeshData.indices.push_back(faceIndices[0]);
			inOutRawMeshData.indices.push_back(faceIndices[triangleIndex]);
			inOutRawMeshData.indices.push_back(faceIndices[triangleIndex + 1]);
		}

		faceIndices.clear();
	}

	return faceIndices.empty();
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

	unordered_map<FbxObjectIdentifier, float4x4> modelWorldMatrixByIdentifier = {};
	auto buildModelWorldMatrix = [&](const auto& recursiveBuild, const FbxObjectIdentifier modelIdentifier, float4x4& outWorldMatrix) -> bool
	{
		auto foundCachedMatrix = modelWorldMatrixByIdentifier.find(modelIdentifier);
		if (foundCachedMatrix != modelWorldMatrixByIdentifier.end())
		{
			outWorldMatrix = foundCachedMatrix->second;
			return true;
		}

		auto foundModel = sceneData.modelByIdentifier.find(modelIdentifier);
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

		modelWorldMatrixByIdentifier.emplace(modelIdentifier, outWorldMatrix);
		return true;
	};

	for (auto geometryIterator = sceneData.geometryByIdentifier.begin();
		geometryIterator != sceneData.geometryByIdentifier.end();
		++geometryIterator)
	{
		const FbxObjectIdentifier geometryIdentifier = geometryIterator->first;
		const FbxGeometryData& geometryData = geometryIterator->second;
		auto foundModelIdentifiers = sceneData.geometryToModelIdentifiers.find(geometryIdentifier);
		if (foundModelIdentifiers == sceneData.geometryToModelIdentifiers.end())
		{
			continue;
		}

		for (uint32 modelIndex = 0; modelIndex < static_cast<uint32>(foundModelIdentifiers->second.size()); ++modelIndex)
		{
			const FbxObjectIdentifier modelIdentifier = foundModelIdentifiers->second[modelIndex];
			float4x4 worldMatrix = {};
			if (!buildModelWorldMatrix(buildModelWorldMatrix, modelIdentifier, worldMatrix))
			{
				continue;
			}

			float3x3 normalMatrix = {};
			if (!buildInverseTranspose3x3(worldMatrix, normalMatrix))
			{
				buildFallbackNormalMatrix(worldMatrix, normalMatrix);
			}

			const uint32 sectionStartIndex = static_cast<uint32>(rawMeshData.indices.size());
			if (!appendFbxGeometryToRawMeshData(geometryData, worldMatrix, normalMatrix, rawMeshData))
			{
				continue;
			}

			const uint32 sectionIndexCount = static_cast<uint32>(rawMeshData.indices.size()) - sectionStartIndex;
			outMeshAsset.addSectionRange(lodLevel, sectionStartIndex, sectionIndexCount);
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
