#include "Engine/Module/MeshParser/ObjMeshParser.h"

#include <fstream>
#include <sstream>

struct ObjFloat2
{
	float x = 0.0f;
	float y = 0.0f;
};

struct ObjFloat3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct ObjFaceVertex
{
	int32 positionIndex = 0;
	int32 textureIndex = 0;
	int32 normalIndex = 0;

	bool operator==(const ObjFaceVertex& other) const
	{
		return positionIndex == other.positionIndex
			&& textureIndex == other.textureIndex
			&& normalIndex == other.normalIndex;
	}
};

struct ObjFaceVertexHasher
{
	size_t operator()(const ObjFaceVertex& value) const
	{
		size_t hashValue = static_cast<size_t>(value.positionIndex);
		hashValue = (hashValue * 16777619u) ^ static_cast<size_t>(value.textureIndex);
		hashValue = (hashValue * 16777619u) ^ static_cast<size_t>(value.normalIndex);
		return hashValue;
	}
};

static bool parseObjIndexValue(
	const string& tokenText,
	int32& parsedIndex)
{
	if (tokenText.empty())
	{
		parsedIndex = 0;
		return true;
	}

	string_input_stream parser(tokenText);
	int32 value = 0;
	parser >> value;
	if (!parser || !parser.eof())
	{
		return false;
	}

	parsedIndex = value;
	return true;
}

static bool parseObjFaceVertexToken(
	const string& tokenText,
	ObjFaceVertex& outFaceVertex)
{
	outFaceVertex = {};
	if (tokenText.empty())
	{
		return false;
	}

	size_t firstSlash = tokenText.find('/');
	if (firstSlash == string::npos)
	{
		return parseObjIndexValue(tokenText, outFaceVertex.positionIndex);
	}

	const size_t secondSlash = tokenText.find('/', firstSlash + 1);
	if (!parseObjIndexValue(tokenText.substr(0, firstSlash), outFaceVertex.positionIndex))
	{
		return false;
	}

	if (secondSlash == string::npos)
	{
		return parseObjIndexValue(tokenText.substr(firstSlash + 1), outFaceVertex.textureIndex);
	}

	if (!parseObjIndexValue(
		tokenText.substr(firstSlash + 1, secondSlash - firstSlash - 1),
		outFaceVertex.textureIndex))
	{
		return false;
	}

	return parseObjIndexValue(tokenText.substr(secondSlash + 1), outFaceVertex.normalIndex);
}

static bool tryResolveObjIndex(
	const int32 objIndexValue,
	const size_t sourceCount,
	uint32& resolvedIndex)
{
	if (objIndexValue > 0)
	{
		const long long indexValue = static_cast<long long>(objIndexValue) - 1;
		if (indexValue < 0 || indexValue >= static_cast<long long>(sourceCount))
		{
			return false;
		}

		resolvedIndex = static_cast<uint32>(indexValue);
		return true;
	}

	if (objIndexValue < 0)
	{
		const long long indexFromEnd = static_cast<long long>(sourceCount) + objIndexValue;
		if (indexFromEnd < 0 || indexFromEnd >= static_cast<long long>(sourceCount))
		{
			return false;
		}

		resolvedIndex = static_cast<uint32>(indexFromEnd);
		return true;
	}

	return false;
}

static bool failObjLoad(
	const string& meshFilePath,
	const uint32 lineNumber,
	const string& reason,
	string& outErrorText)
{
	unused(meshFilePath);
	unused(lineNumber);
	outErrorText = reason;
	return false;
}

bool ObjMeshParser::parse(
	const string& meshFilePath,
	const uint32 lodLevel,
	MeshAsset& outMeshAsset,
	string& outErrorText) const
{
	outMeshAsset = {};
	outErrorText.clear();
	RawMeshData& rawMeshData = outMeshAsset.getRawMeshData();

	input_file_stream fileStream(meshFilePath);
	if (!fileStream.is_open())
	{
		outErrorText = "file_open_failed";
		return false;
	}

	vector<ObjFloat3> sourcePositions;
	vector<ObjFloat3> sourceNormals;
	vector<ObjFloat2> sourceTexcoords;
	unordered_map<ObjFaceVertex, uint32, ObjFaceVertexHasher> vertexMap;

	string lineText = {};
	uint32 lineNumber = 0;
	while (std::getline(fileStream, lineText))
	{
		++lineNumber;
		if (lineText.empty())
		{
			continue;
		}

		string_input_stream lineParser(lineText);
		string token = {};
		lineParser >> token;
		if (!lineParser || token.empty() || token[0] == '#')
		{
			continue;
		}

		if (token == "v")
		{
			ObjFloat3 position = {};
			lineParser >> position.x >> position.y >> position.z;
			if (!lineParser)
			{
				return failObjLoad(meshFilePath, lineNumber, "invalid_position", outErrorText);
			}

			sourcePositions.push_back(position);
			continue;
		}

		if (token == "vt")
		{
			ObjFloat2 texcoord = {};
			lineParser >> texcoord.x >> texcoord.y;
			if (!lineParser)
			{
				return failObjLoad(meshFilePath, lineNumber, "invalid_texcoord", outErrorText);
			}

			sourceTexcoords.push_back(texcoord);
			continue;
		}

		if (token == "vn")
		{
			ObjFloat3 normal = {};
			lineParser >> normal.x >> normal.y >> normal.z;
			if (!lineParser)
			{
				return failObjLoad(meshFilePath, lineNumber, "invalid_normal", outErrorText);
			}

			sourceNormals.push_back(normal);
			continue;
		}

		if (token != "f")
		{
			continue;
		}

		vector<string> faceTokens;
		string faceToken = {};
		while (lineParser >> faceToken)
		{
			faceTokens.push_back(faceToken);
		}

		if (faceTokens.size() < 3)
		{
			return failObjLoad(meshFilePath, lineNumber, "invalid_face_vertex_count", outErrorText);
		}

		vector<uint32> faceIndices;
		faceIndices.reserve(faceTokens.size());
		for (uint32 faceVertexIndex = 0; faceVertexIndex < static_cast<uint32>(faceTokens.size()); ++faceVertexIndex)
		{
			ObjFaceVertex faceVertex = {};
			if (!parseObjFaceVertexToken(faceTokens[faceVertexIndex], faceVertex))
			{
				return failObjLoad(meshFilePath, lineNumber, "invalid_face_vertex_token", outErrorText);
			}

			auto foundVertex = vertexMap.find(faceVertex);
			if (foundVertex != vertexMap.end())
			{
				faceIndices.push_back(foundVertex->second);
				continue;
			}

			uint32 resolvedPositionIndex = 0;
			if (!tryResolveObjIndex(faceVertex.positionIndex, sourcePositions.size(), resolvedPositionIndex))
			{
				return failObjLoad(meshFilePath, lineNumber, "position_index_out_of_range", outErrorText);
			}

			PositionData positionVertex = {};
			const ObjFloat3& sourcePosition = sourcePositions[resolvedPositionIndex];
			positionVertex.x = sourcePosition.x;
			positionVertex.y = sourcePosition.y;
			positionVertex.z = sourcePosition.z;

			NormalData normalVertex = {};
			TexcoordData texcoordVertex = {};

			if (faceVertex.normalIndex != 0)
			{
				uint32 resolvedNormalIndex = 0;
				if (!tryResolveObjIndex(faceVertex.normalIndex, sourceNormals.size(), resolvedNormalIndex))
				{
					return failObjLoad(meshFilePath, lineNumber, "normal_index_out_of_range", outErrorText);
				}

				const ObjFloat3& sourceNormal = sourceNormals[resolvedNormalIndex];
				normalVertex.x = sourceNormal.x;
				normalVertex.y = sourceNormal.y;
				normalVertex.z = sourceNormal.z;
			}

			if (faceVertex.textureIndex != 0)
			{
				uint32 resolvedTexcoordIndex = 0;
				if (!tryResolveObjIndex(faceVertex.textureIndex, sourceTexcoords.size(), resolvedTexcoordIndex))
				{
					return failObjLoad(meshFilePath, lineNumber, "texcoord_index_out_of_range", outErrorText);
				}

				const ObjFloat2& sourceTexcoord = sourceTexcoords[resolvedTexcoordIndex];
				texcoordVertex.x = sourceTexcoord.x;
				texcoordVertex.y = sourceTexcoord.y;
			}

			const uint32 newVertexIndex = static_cast<uint32>(rawMeshData.positionVertices.size());
			rawMeshData.positionVertices.push_back(positionVertex);
			rawMeshData.normalVertices.push_back(normalVertex);
			rawMeshData.texcoordVertices.push_back(texcoordVertex);
			vertexMap.emplace(faceVertex, newVertexIndex);
			faceIndices.push_back(newVertexIndex);
		}

		for (uint32 triangleIndex = 1; triangleIndex + 1 < static_cast<uint32>(faceIndices.size()); ++triangleIndex)
		{
			rawMeshData.indices.push_back(faceIndices[0]);
			rawMeshData.indices.push_back(faceIndices[triangleIndex]);
			rawMeshData.indices.push_back(faceIndices[triangleIndex + 1]);
		}
	}

	if (rawMeshData.positionVertices.empty() || rawMeshData.indices.empty())
	{
		outErrorText = "mesh_data_empty";
		return false;
	}

	if (rawMeshData.normalVertices.size() != rawMeshData.positionVertices.size()
		|| rawMeshData.texcoordVertices.size() != rawMeshData.positionVertices.size())
	{
		outErrorText = "mesh_vertex_stream_mismatch";
		return false;
	}

	outMeshAsset.setName(meshFilePath + ":LOD" + std::to_string(lodLevel));
	outMeshAsset.setSource(meshFilePath);
	return true;
}
