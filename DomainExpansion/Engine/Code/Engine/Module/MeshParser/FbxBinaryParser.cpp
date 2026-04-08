#include "Engine/Module/MeshParser/FbxBinaryParser.h"
#include "Engine/Common/StringSlice.h"

#include "ThirdParty/Zlib/zlib.h"

#include <cstring>

static constexpr uint32 fbxBinaryHeaderVersionOffset = 23;
static constexpr uint32 fbxBinaryHeaderSizeInBytes = 27;
static constexpr uint32 fbxLegacyNodeHeaderSizeInBytes = 13;
static constexpr uint32 fbxModernNodeHeaderSizeInBytes = 25;

struct FbxProperty
{
	char type = 0;
	FbxObjectIdentifier integerValue = 0;
	double floatingValue = 0.0;
	string stringValue = {};
	vector<char> binaryValue = {};
	uint32 arrayElementCount = 0;
};

struct FbxNode
{
	string name = {};
	vector<FbxProperty> properties = {};
	size_t contentOffset = 0;
	uint64 endOffset = 0;
	bool nullNode = false;
};

static bool failFbxLoad(
	const string& meshFilePath,
	const string& reason,
	string& outErrorText)
{
	outErrorText = reason;
	error << "[MeshParser][Error] path=" << meshFilePath
		  << " reason=" << outErrorText << lineBreak;
	return false;
}

template <typename type_name>
static bool readFbxValue(
	const vector<char>& binaryData,
	const size_t readOffset,
	type_name& outValue)
{
	const bool validReadRange = readOffset <= binaryData.size()
		&& sizeof(type_name) <= binaryData.size() - readOffset;
	if (!validReadRange)
	{
		return false;
	}

	memcpy(&outValue, binaryData.data() + readOffset, sizeof(type_name));
	return true;
}

static string sanitizeFbxText(const string& text)
{
	const size_t nullTerminatorIndex = text.find('\0');
	if (nullTerminatorIndex != string::npos)
	{
		return sliceString(text, 0, nullTerminatorIndex);
	}

	return text;
}

static bool decompressFbxArrayData(
	const char* compressedBytes,
	const uint32 compressedByteCount,
	const uint64 expectedDecompressedByteCount,
	vector<char>& outBinaryData)
{
	outBinaryData.clear();
	if (expectedDecompressedByteCount == 0)
	{
		return true;
	}

	const bool validCompressedBuffer = compressedBytes != nullptr
		&& compressedByteCount > 0
		&& expectedDecompressedByteCount <= static_cast<uint64>(uint32MaxValue);
	if (!validCompressedBuffer)
	{
		return false;
	}

	outBinaryData.resize(static_cast<size_t>(expectedDecompressedByteCount));
	uLongf destinationByteCount = static_cast<uLongf>(expectedDecompressedByteCount);
	const int32 uncompressResult = uncompress(
		reinterpret_cast<Bytef*>(outBinaryData.data()),
		&destinationByteCount,
		reinterpret_cast<const Bytef*>(compressedBytes),
		static_cast<uLong>(compressedByteCount));
	if (uncompressResult != Z_OK || destinationByteCount != static_cast<uLongf>(expectedDecompressedByteCount))
	{
		outBinaryData.clear();
		return false;
	}

	return true;
}

static uint32 getFbxArrayElementSizeInBytes(const char propertyType)
{
	switch (propertyType)
	{
	case 'f':
	case 'i':
		return 4;
	case 'd':
	case 'l':
		return 8;
	case 'b':
	case 'c':
		return 1;
	default:
		return 0;
	}
}

static bool parseFbxProperty(
	const vector<char>& binaryData,
	size_t& inOutReadOffset,
	FbxProperty& outProperty)
{
	outProperty = {};

	char propertyType = 0;
	if (!readFbxValue(binaryData, inOutReadOffset, propertyType))
	{
		return false;
	}

	++inOutReadOffset;
	outProperty.type = propertyType;
	switch (propertyType)
	{
	case 'Y':
	{
		int16_t value = 0;
		if (!readFbxValue(binaryData, inOutReadOffset, value))
		{
			return false;
		}

		outProperty.integerValue = static_cast<FbxObjectIdentifier>(value);
		inOutReadOffset += sizeof(value);
		return true;
	}
	case 'C':
	case 'B':
	{
		char value = 0;
		if (!readFbxValue(binaryData, inOutReadOffset, value))
		{
			return false;
		}

		outProperty.integerValue = static_cast<FbxObjectIdentifier>(value);
		inOutReadOffset += sizeof(value);
		return true;
	}
	case 'I':
	{
		int32 value = 0;
		if (!readFbxValue(binaryData, inOutReadOffset, value))
		{
			return false;
		}

		outProperty.integerValue = static_cast<FbxObjectIdentifier>(value);
		inOutReadOffset += sizeof(value);
		return true;
	}
	case 'L':
	{
		if (!readFbxValue(binaryData, inOutReadOffset, outProperty.integerValue))
		{
			return false;
		}

		inOutReadOffset += sizeof(outProperty.integerValue);
		return true;
	}
	case 'F':
	{
		float value = 0.0f;
		if (!readFbxValue(binaryData, inOutReadOffset, value))
		{
			return false;
		}

		outProperty.floatingValue = static_cast<double>(value);
		inOutReadOffset += sizeof(value);
		return true;
	}
	case 'D':
	{
		if (!readFbxValue(binaryData, inOutReadOffset, outProperty.floatingValue))
		{
			return false;
		}

		inOutReadOffset += sizeof(outProperty.floatingValue);
		return true;
	}
	case 'S':
	case 'R':
	{
		uint32 byteCount = 0;
		if (!readFbxValue(binaryData, inOutReadOffset, byteCount))
		{
			return false;
		}

		inOutReadOffset += sizeof(byteCount);
		const bool validReadRange = inOutReadOffset <= binaryData.size()
			&& byteCount <= binaryData.size() - inOutReadOffset;
		if (!validReadRange)
		{
			return false;
		}

		outProperty.stringValue.assign(binaryData.data() + inOutReadOffset, static_cast<size_t>(byteCount));
		inOutReadOffset += static_cast<size_t>(byteCount);
		return true;
	}
	case 'f':
	case 'd':
	case 'i':
	case 'l':
	case 'b':
	case 'c':
	{
		uint32 arrayLength = 0;
		uint32 encoding = 0;
		uint32 storedByteCount = 0;
		if (!readFbxValue(binaryData, inOutReadOffset, arrayLength)
			|| !readFbxValue(binaryData, inOutReadOffset + sizeof(arrayLength), encoding)
			|| !readFbxValue(binaryData, inOutReadOffset + sizeof(arrayLength) + sizeof(encoding), storedByteCount))
		{
			return false;
		}

		inOutReadOffset += sizeof(arrayLength) + sizeof(encoding) + sizeof(storedByteCount);
		const uint32 elementSizeInBytes = getFbxArrayElementSizeInBytes(propertyType);
		const uint64 expectedByteCount = static_cast<uint64>(arrayLength) * elementSizeInBytes;
		const bool validStoredRange = inOutReadOffset <= binaryData.size()
			&& storedByteCount <= binaryData.size() - inOutReadOffset
			&& elementSizeInBytes > 0;
		if (!validStoredRange)
		{
			return false;
		}

		const char* storedBytes = binaryData.data() + inOutReadOffset;
		if (encoding == 0)
		{
			if (expectedByteCount != static_cast<uint64>(storedByteCount))
			{
				return false;
			}

			outProperty.binaryValue.resize(static_cast<size_t>(storedByteCount));
			if (storedByteCount > 0)
			{
				memcpy(outProperty.binaryValue.data(), storedBytes, static_cast<size_t>(storedByteCount));
			}
		}
		else if (encoding == 1)
		{
			if (!decompressFbxArrayData(
				storedBytes,
				storedByteCount,
				expectedByteCount,
				outProperty.binaryValue))
			{
				return false;
			}
		}
		else
		{
			return false;
		}

		outProperty.arrayElementCount = arrayLength;
		inOutReadOffset += static_cast<size_t>(storedByteCount);
		return true;
	}
	default:
		return false;
	}
}

static bool parseFbxNode(
	const vector<char>& binaryData,
	const uint32 fbxVersion,
	size_t& inOutReadOffset,
	FbxNode& outNode)
{
	outNode = {};

	uint64 endOffset = 0;
	uint64 propertyCount = 0;
	uint64 propertyListLength = 0;
	unsigned char nameLength = 0;
	if (fbxVersion >= 7500)
	{
		if (!readFbxValue(binaryData, inOutReadOffset, endOffset)
			|| !readFbxValue(binaryData, inOutReadOffset + sizeof(endOffset), propertyCount)
			|| !readFbxValue(binaryData, inOutReadOffset + (sizeof(endOffset) * 2), propertyListLength)
			|| !readFbxValue(binaryData, inOutReadOffset + (sizeof(endOffset) * 3), nameLength))
		{
			return false;
		}

		inOutReadOffset += fbxModernNodeHeaderSizeInBytes;
	}
	else
	{
		uint32 legacyEndOffset = 0;
		uint32 legacyPropertyCount = 0;
		uint32 legacyPropertyListLength = 0;
		if (!readFbxValue(binaryData, inOutReadOffset, legacyEndOffset)
			|| !readFbxValue(binaryData, inOutReadOffset + sizeof(legacyEndOffset), legacyPropertyCount)
			|| !readFbxValue(binaryData, inOutReadOffset + (sizeof(legacyEndOffset) * 2), legacyPropertyListLength)
			|| !readFbxValue(binaryData, inOutReadOffset + (sizeof(legacyEndOffset) * 3), nameLength))
		{
			return false;
		}

		endOffset = legacyEndOffset;
		propertyCount = legacyPropertyCount;
		propertyListLength = legacyPropertyListLength;
		inOutReadOffset += fbxLegacyNodeHeaderSizeInBytes;
	}

	if (endOffset == 0 && propertyCount == 0 && propertyListLength == 0 && nameLength == 0)
	{
		outNode.nullNode = true;
		return true;
	}

	const bool validNodeEndOffset = endOffset <= binaryData.size()
		&& endOffset >= inOutReadOffset;
	if (!validNodeEndOffset)
	{
		return false;
	}

	const bool validNameRange = inOutReadOffset <= binaryData.size()
		&& nameLength <= binaryData.size() - inOutReadOffset;
	if (!validNameRange)
	{
		return false;
	}

	outNode.endOffset = endOffset;
	outNode.name.assign(binaryData.data() + inOutReadOffset, static_cast<size_t>(nameLength));
	inOutReadOffset += static_cast<size_t>(nameLength);
	outNode.properties.resize(static_cast<size_t>(propertyCount));

	const size_t propertyDataOffset = inOutReadOffset;
	for (uint64 propertyIndex = 0; propertyIndex < propertyCount; ++propertyIndex)
	{
		if (!parseFbxProperty(binaryData, inOutReadOffset, outNode.properties[static_cast<size_t>(propertyIndex)]))
		{
			return false;
		}
	}

	if (static_cast<uint64>(inOutReadOffset - propertyDataOffset) != propertyListLength)
	{
		return false;
	}

	outNode.contentOffset = inOutReadOffset;
	return true;
}

template <typename value_type>
static bool copyFbxBinaryArrayToVector(
	const FbxProperty& property,
	const char expectedPropertyType,
	vector<value_type>& outValues)
{
	outValues.clear();
	if (property.type != expectedPropertyType)
	{
		return false;
	}

	const uint64 expectedByteCount = static_cast<uint64>(property.arrayElementCount) * sizeof(value_type);
	if (property.binaryValue.size() != static_cast<size_t>(expectedByteCount))
	{
		return false;
	}

	outValues.resize(property.arrayElementCount);
	if (expectedByteCount > 0)
	{
		memcpy(outValues.data(), property.binaryValue.data(), static_cast<size_t>(expectedByteCount));
	}

	return true;
}

static bool parseFbxLayerElementNormal(
	const vector<char>& binaryData,
	const uint32 fbxVersion,
	const FbxNode& layerElementNode,
	FbxLayerElementFloat3& outNormals)
{
	outNormals = {};

	size_t readOffset = layerElementNode.contentOffset;
	while (readOffset < static_cast<size_t>(layerElementNode.endOffset))
	{
		FbxNode childNode = {};
		if (!parseFbxNode(binaryData, fbxVersion, readOffset, childNode))
		{
			return false;
		}

		if (childNode.nullNode)
		{
			break;
		}

		if (childNode.name == "MappingInformationType" && childNode.properties.size() == 1)
		{
			outNormals.mappingInformationType = childNode.properties[0].stringValue;
		}
		else if (childNode.name == "ReferenceInformationType" && childNode.properties.size() == 1)
		{
			outNormals.referenceInformationType = childNode.properties[0].stringValue;
		}
		else if (childNode.name == "Normals" && childNode.properties.size() == 1)
		{
			if (!copyFbxBinaryArrayToVector(childNode.properties[0], 'd', outNormals.directValues))
			{
				return false;
			}
		}
		else if ((childNode.name == "NormalsIndex" || childNode.name == "NormalIndex") && childNode.properties.size() == 1)
		{
			if (!copyFbxBinaryArrayToVector(childNode.properties[0], 'i', outNormals.indices))
			{
				return false;
			}
		}

		readOffset = static_cast<size_t>(childNode.endOffset);
	}

	return true;
}

static bool parseFbxLayerElementUV(
	const vector<char>& binaryData,
	const uint32 fbxVersion,
	const FbxNode& layerElementNode,
	FbxLayerElementFloat2& outTexcoords)
{
	outTexcoords = {};

	size_t readOffset = layerElementNode.contentOffset;
	while (readOffset < static_cast<size_t>(layerElementNode.endOffset))
	{
		FbxNode childNode = {};
		if (!parseFbxNode(binaryData, fbxVersion, readOffset, childNode))
		{
			return false;
		}

		if (childNode.nullNode)
		{
			break;
		}

		if (childNode.name == "Name" && childNode.properties.size() == 1)
		{
			outTexcoords.name = sanitizeFbxText(childNode.properties[0].stringValue);
		}
		else if (childNode.name == "MappingInformationType" && childNode.properties.size() == 1)
		{
			outTexcoords.mappingInformationType = childNode.properties[0].stringValue;
		}
		else if (childNode.name == "ReferenceInformationType" && childNode.properties.size() == 1)
		{
			outTexcoords.referenceInformationType = childNode.properties[0].stringValue;
		}
		else if (childNode.name == "UV" && childNode.properties.size() == 1)
		{
			if (!copyFbxBinaryArrayToVector(childNode.properties[0], 'd', outTexcoords.directValues))
			{
				return false;
			}
		}
		else if (childNode.name == "UVIndex" && childNode.properties.size() == 1)
		{
			if (!copyFbxBinaryArrayToVector(childNode.properties[0], 'i', outTexcoords.indices))
			{
				return false;
			}
		}

		readOffset = static_cast<size_t>(childNode.endOffset);
	}

	return true;
}

static bool parseFbxLayerElementMaterial(
	const vector<char>& binaryData,
	const uint32 fbxVersion,
	const FbxNode& layerElementNode,
	FbxLayerElementMaterial& outMaterials)
{
	outMaterials = {};

	size_t readOffset = layerElementNode.contentOffset;
	while (readOffset < static_cast<size_t>(layerElementNode.endOffset))
	{
		FbxNode childNode = {};
		if (!parseFbxNode(binaryData, fbxVersion, readOffset, childNode))
		{
			return false;
		}

		if (childNode.nullNode)
		{
			break;
		}

		if (childNode.name == "MappingInformationType" && childNode.properties.size() == 1)
		{
			outMaterials.mappingInformationType = childNode.properties[0].stringValue;
		}
		else if (childNode.name == "ReferenceInformationType" && childNode.properties.size() == 1)
		{
			outMaterials.referenceInformationType = childNode.properties[0].stringValue;
		}
		else if (childNode.name == "Materials" && childNode.properties.size() == 1)
		{
			if (!copyFbxBinaryArrayToVector(childNode.properties[0], 'i', outMaterials.materialIndices))
			{
				return false;
			}
		}

		readOffset = static_cast<size_t>(childNode.endOffset);
	}

	return true;
}

static bool parseFbxGeometryNode(
	const vector<char>& binaryData,
	const uint32 fbxVersion,
	const FbxNode& geometryNode,
	FbxGeometryData& outGeometryData,
	FbxObjectIdentifier& outGeometryIdentifier)
{
	outGeometryData = {};
	outGeometryIdentifier = 0;

	const bool validGeometryProperties = geometryNode.properties.size() >= 3
		&& geometryNode.properties[0].type == 'L'
		&& geometryNode.properties[1].type == 'S'
		&& geometryNode.properties[2].type == 'S'
		&& geometryNode.properties[2].stringValue == "Mesh";
	if (!validGeometryProperties)
	{
		return false;
	}

	outGeometryIdentifier = geometryNode.properties[0].integerValue;
	outGeometryData.name = sanitizeFbxText(geometryNode.properties[1].stringValue);

	size_t readOffset = geometryNode.contentOffset;
	while (readOffset < static_cast<size_t>(geometryNode.endOffset))
	{
		FbxNode childNode = {};
		if (!parseFbxNode(binaryData, fbxVersion, readOffset, childNode))
		{
			return false;
		}

		if (childNode.nullNode)
		{
			break;
		}

		if (childNode.name == "Vertices" && childNode.properties.size() == 1)
		{
			if (!copyFbxBinaryArrayToVector(childNode.properties[0], 'd', outGeometryData.vertices))
			{
				return false;
			}
		}
		else if (childNode.name == "PolygonVertexIndex" && childNode.properties.size() == 1)
		{
			if (!copyFbxBinaryArrayToVector(childNode.properties[0], 'i', outGeometryData.polygonVertexIndices))
			{
				return false;
			}
		}
		else if (childNode.name == "LayerElementNormal")
		{
			if (!parseFbxLayerElementNormal(binaryData, fbxVersion, childNode, outGeometryData.normals))
			{
				return false;
			}
		}
		else if (childNode.name == "LayerElementUV")
		{
			FbxLayerElementFloat2 texcoordLayer = {};
			if (!parseFbxLayerElementUV(binaryData, fbxVersion, childNode, texcoordLayer))
			{
				return false;
			}

			outGeometryData.uvLayers.push_back(moveValue(texcoordLayer));
		}
		else if (childNode.name == "LayerElementMaterial")
		{
			if (!parseFbxLayerElementMaterial(binaryData, fbxVersion, childNode, outGeometryData.materials))
			{
				return false;
			}
		}

		readOffset = static_cast<size_t>(childNode.endOffset);
	}

	return !outGeometryData.vertices.empty() && !outGeometryData.polygonVertexIndices.empty();
}

static double getFbxPropertyDoubleValue(const FbxProperty& property)
{
	if (property.type == 'D' || property.type == 'F')
	{
		return property.floatingValue;
	}

	return static_cast<double>(property.integerValue);
}

static bool parseFbxModelNode(
	const vector<char>& binaryData,
	const uint32 fbxVersion,
	const FbxNode& modelNode,
	FbxModelData& outModelData,
	FbxObjectIdentifier& outModelIdentifier)
{
	outModelData = {};
	outModelIdentifier = 0;

	const bool validModelProperties = modelNode.properties.size() >= 3
		&& modelNode.properties[0].type == 'L'
		&& modelNode.properties[1].type == 'S'
		&& modelNode.properties[2].type == 'S';
	if (!validModelProperties)
	{
		return false;
	}

	outModelIdentifier = modelNode.properties[0].integerValue;
	outModelData.name = sanitizeFbxText(modelNode.properties[1].stringValue);
	outModelData.modelType = sanitizeFbxText(modelNode.properties[2].stringValue);

	size_t readOffset = modelNode.contentOffset;
	while (readOffset < static_cast<size_t>(modelNode.endOffset))
	{
		FbxNode childNode = {};
		if (!parseFbxNode(binaryData, fbxVersion, readOffset, childNode))
		{
			return false;
		}

		if (childNode.nullNode)
		{
			break;
		}

		if (childNode.name == "Properties70")
		{
			size_t propertyReadOffset = childNode.contentOffset;
			while (propertyReadOffset < static_cast<size_t>(childNode.endOffset))
			{
				FbxNode propertyNode = {};
				if (!parseFbxNode(binaryData, fbxVersion, propertyReadOffset, propertyNode))
				{
					return false;
				}

				if (propertyNode.nullNode)
				{
					break;
				}

				if (propertyNode.name == "P" && propertyNode.properties.size() >= 7 && propertyNode.properties[0].type == 'S')
				{
					const string propertyName = sanitizeFbxText(propertyNode.properties[0].stringValue);
					if (propertyName == "Lcl Translation")
					{
						outModelData.translation.x = static_cast<float>(getFbxPropertyDoubleValue(propertyNode.properties[4]));
						outModelData.translation.y = static_cast<float>(getFbxPropertyDoubleValue(propertyNode.properties[5]));
						outModelData.translation.z = static_cast<float>(getFbxPropertyDoubleValue(propertyNode.properties[6]));
					}
					else if (propertyName == "Lcl Rotation")
					{
						outModelData.rotationInDegrees.x = static_cast<float>(getFbxPropertyDoubleValue(propertyNode.properties[4]));
						outModelData.rotationInDegrees.y = static_cast<float>(getFbxPropertyDoubleValue(propertyNode.properties[5]));
						outModelData.rotationInDegrees.z = static_cast<float>(getFbxPropertyDoubleValue(propertyNode.properties[6]));
					}
					else if (propertyName == "Lcl Scaling")
					{
						outModelData.scaling.x = static_cast<float>(getFbxPropertyDoubleValue(propertyNode.properties[4]));
						outModelData.scaling.y = static_cast<float>(getFbxPropertyDoubleValue(propertyNode.properties[5]));
						outModelData.scaling.z = static_cast<float>(getFbxPropertyDoubleValue(propertyNode.properties[6]));
					}
				}

				propertyReadOffset = static_cast<size_t>(propertyNode.endOffset);
			}
		}

		readOffset = static_cast<size_t>(childNode.endOffset);
	}

	return true;
}

static bool parseFbxMaterialNode(
	const FbxNode& materialNode,
	FbxMaterialData& outMaterialData,
	FbxObjectIdentifier& outMaterialIdentifier)
{
	outMaterialData = {};
	outMaterialIdentifier = 0;

	const bool validMaterialProperties = materialNode.properties.size() >= 3
		&& materialNode.properties[0].type == 'L'
		&& materialNode.properties[1].type == 'S'
		&& materialNode.properties[2].type == 'S';
	if (!validMaterialProperties)
	{
		return false;
	}

	outMaterialIdentifier = materialNode.properties[0].integerValue;
	outMaterialData.name = sanitizeFbxText(materialNode.properties[1].stringValue);
	outMaterialData.materialType = sanitizeFbxText(materialNode.properties[2].stringValue);
	return true;
}

static bool parseFbxObjectsNode(
	const vector<char>& binaryData,
	const uint32 fbxVersion,
	const FbxNode& objectsNode,
	FbxSceneData& outSceneData)
{
	size_t readOffset = objectsNode.contentOffset;
	while (readOffset < static_cast<size_t>(objectsNode.endOffset))
	{
		FbxNode childNode = {};
		if (!parseFbxNode(binaryData, fbxVersion, readOffset, childNode))
		{
			return false;
		}

		if (childNode.nullNode)
		{
			break;
		}

		if (childNode.name == "Geometry")
		{
			FbxGeometryData geometryData = {};
			FbxObjectIdentifier geometryIdentifier = 0;
			if (parseFbxGeometryNode(binaryData, fbxVersion, childNode, geometryData, geometryIdentifier))
			{
				outSceneData.geometryByIdentifier[geometryIdentifier] = moveValue(geometryData);
			}
		}
		else if (childNode.name == "Model")
		{
			FbxModelData modelData = {};
			FbxObjectIdentifier modelIdentifier = 0;
			if (!parseFbxModelNode(binaryData, fbxVersion, childNode, modelData, modelIdentifier))
			{
				return false;
			}

			outSceneData.modelByIdentifier[modelIdentifier] = moveValue(modelData);
		}
		else if (childNode.name == "Material")
		{
			FbxMaterialData materialData = {};
			FbxObjectIdentifier materialIdentifier = 0;
			if (!parseFbxMaterialNode(childNode, materialData, materialIdentifier))
			{
				return false;
			}

			outSceneData.materialByIdentifier[materialIdentifier] = moveValue(materialData);
		}

		readOffset = static_cast<size_t>(childNode.endOffset);
	}

	return true;
}

static bool parseFbxConnectionsNode(
	const vector<char>& binaryData,
	const uint32 fbxVersion,
	const FbxNode& connectionsNode,
	FbxSceneData& outSceneData)
{
	size_t readOffset = connectionsNode.contentOffset;
	while (readOffset < static_cast<size_t>(connectionsNode.endOffset))
	{
		FbxNode childNode = {};
		if (!parseFbxNode(binaryData, fbxVersion, readOffset, childNode))
		{
			return false;
		}

		if (childNode.nullNode)
		{
			break;
		}

		const bool validConnectionNode = childNode.name == "C"
			&& childNode.properties.size() >= 3
			&& childNode.properties[0].type == 'S'
			&& childNode.properties[1].type == 'L'
			&& childNode.properties[2].type == 'L';
		if (validConnectionNode)
		{
			const string connectionType = sanitizeFbxText(childNode.properties[0].stringValue);
			const FbxObjectIdentifier childIdentifier = childNode.properties[1].integerValue;
			const FbxObjectIdentifier parentIdentifier = childNode.properties[2].integerValue;
			FbxConnectionData connectionData = {
				.connectionType = connectionType,
				.childIdentifier = childIdentifier,
				.parentIdentifier = parentIdentifier,
			};
			if (connectionType == "OP" && childNode.properties.size() >= 4 && childNode.properties[3].type == 'S')
			{
				connectionData.propertyName = sanitizeFbxText(childNode.properties[3].stringValue);
			}

			outSceneData.connections.push_back(moveValue(connectionData));

			if (connectionType == "OO")
			{
				auto foundGeometry = outSceneData.geometryByIdentifier.find(childIdentifier);
				if (foundGeometry != outSceneData.geometryByIdentifier.end())
				{
					outSceneData.geometryToModelIdentifiers[childIdentifier].push_back(parentIdentifier);
					outSceneData.modelToGeometryIdentifiers[parentIdentifier].push_back(childIdentifier);
				}

				auto foundModel = outSceneData.modelByIdentifier.find(childIdentifier);
				if (foundModel != outSceneData.modelByIdentifier.end())
				{
					foundModel->second.parentModelIdentifier = parentIdentifier;
				}

				auto foundMaterial = outSceneData.materialByIdentifier.find(childIdentifier);
				if (foundMaterial != outSceneData.materialByIdentifier.end())
				{
					outSceneData.modelToMaterialIdentifiers[parentIdentifier].push_back(childIdentifier);
				}
			}
		}

		readOffset = static_cast<size_t>(childNode.endOffset);
	}

	return true;
}

bool parseFbxSceneData(
	const string& meshFilePath,
	FbxSceneData& outSceneData,
	string& outErrorText)
{
	outSceneData = {};

	input_file_stream fileStream(meshFilePath, input_file_stream::binary | input_file_stream::ate);
	if (!fileStream.is_open())
	{
		return failFbxLoad(meshFilePath, "file_open_failed", outErrorText);
	}

	const stream_position fileSize = fileStream.tellg();
	if (fileSize <= 0)
	{
		return failFbxLoad(meshFilePath, "file_read_failed", outErrorText);
	}

	vector<char> binaryData(static_cast<size_t>(fileSize));
	fileStream.seekg(0, input_file_stream::beg);
	fileStream.read(binaryData.data(), fileSize);
	if (!fileStream)
	{
		return failFbxLoad(meshFilePath, "file_read_failed", outErrorText);
	}

	const bool validBinaryHeader = binaryData.size() >= fbxBinaryHeaderSizeInBytes
		&& memcmp(binaryData.data(), "Kaydara FBX Binary", 18) == 0;
	if (!validBinaryHeader)
	{
		return failFbxLoad(meshFilePath, "fbx_binary_header_invalid", outErrorText);
	}

	uint32 fbxVersion = 0;
	if (!readFbxValue(binaryData, fbxBinaryHeaderVersionOffset, fbxVersion))
	{
		return failFbxLoad(meshFilePath, "fbx_version_read_failed", outErrorText);
	}

	size_t readOffset = fbxBinaryHeaderSizeInBytes;
	while (readOffset < binaryData.size())
	{
		FbxNode topLevelNode = {};
		if (!parseFbxNode(binaryData, fbxVersion, readOffset, topLevelNode))
		{
			return failFbxLoad(meshFilePath, "fbx_node_parse_failed", outErrorText);
		}

		if (topLevelNode.nullNode)
		{
			break;
		}

		if (topLevelNode.name == "Objects")
		{
			if (!parseFbxObjectsNode(binaryData, fbxVersion, topLevelNode, outSceneData))
			{
				return failFbxLoad(meshFilePath, "fbx_objects_parse_failed", outErrorText);
			}
		}
		else if (topLevelNode.name == "Connections")
		{
			if (!parseFbxConnectionsNode(binaryData, fbxVersion, topLevelNode, outSceneData))
			{
				return failFbxLoad(meshFilePath, "fbx_connections_parse_failed", outErrorText);
			}
		}

		readOffset = static_cast<size_t>(topLevelNode.endOffset);
	}

	if (outSceneData.geometryByIdentifier.empty())
	{
		return failFbxLoad(meshFilePath, "fbx_geometry_missing", outErrorText);
	}

	return true;
}
