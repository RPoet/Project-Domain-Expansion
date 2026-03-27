#include "Engine/Assets/MeshAsset.h"
#include "Engine/Common/FileStream.h"
#include "Engine/Common/Container/Vector.h"
#include "Engine/Common/XML/XML.h"

void RawMeshData::empty()
{
	positionVertices.clear();
	normalVertices.clear();
	texcoordVertices.clear();
	indices.clear();
}

void RawMeshData::serialize(OutputFileStream& fileStream) const
{
	assert(isValid() && "[MeshAsset][Assert] reason=raw_mesh_data_invalid");
	fileStream << positionVertices;
	fileStream << normalVertices;
	fileStream << texcoordVertices;
	fileStream << indices;
}

void RawMeshData::deserialize(InputFileStream& fileStream)
{
	empty();
	fileStream >> positionVertices;
	fileStream >> normalVertices;
	fileStream >> texcoordVertices;
	fileStream >> indices;
	assert(isValid() && "[MeshAsset][Assert] reason=raw_mesh_data_invalid");
}

bool RawMeshData::isValid() const
{
	return positionVertices.size() == normalVertices.size() && positionVertices.size() == texcoordVertices.size();
}

void MeshAsset::empty()
{
	meshes.clear();
	source.clear();
}

vector<RawMeshData>& MeshAsset::getMeshes()
{
	return meshes;
}

const vector<RawMeshData>& MeshAsset::getMeshes() const
{
	return meshes;
}

void MeshAsset::ensureLODCount(const uint32 lodCount)
{
	if (meshes.size() < lodCount)
	{
		meshes.resize(lodCount);
	}
}

uint32 MeshAsset::getLODCount() const
{
	return static_cast<uint32>(meshes.size());
}

RawMeshData& MeshAsset::getRawMeshData(const uint32 lodLevel)
{
	ensureLODCount(lodLevel + 1);
	return meshes[lodLevel];
}

const RawMeshData& MeshAsset::getRawMeshData(const uint32 lodLevel) const
{
	const bool validLODIndex = lodLevel < meshes.size();
	assert(validLODIndex && "[MeshAsset][Assert] reason=lod_index_out_of_range");
	return meshes[lodLevel];
}

uint32 MeshAsset::getVertexCount(const uint32 lodLevel) const
{
	return static_cast<uint32>(getRawMeshData(lodLevel).positionVertices.size());
}

uint32 MeshAsset::getIndexCount(const uint32 lodLevel) const
{
	return static_cast<uint32>(getRawMeshData(lodLevel).indices.size());
}

const string& MeshAsset::getSource() const
{
	return source;
}

void MeshAsset::setSource(const string& inSource)
{
	source = inSource;
}

void MeshAsset::writeAssetProperty(OutputFileStream& fileStream) const
{
	XML& xml = XML::get();
	xml.writeProperty(fileStream, "version", version);
	xml.writeProperty(fileStream, "source", source);
}

void MeshAsset::readAssetProperty(const XMLKeyValueDocument& document)
{
	XML& xml = XML::get();

	uint32 documentVersion = uint32MaxValue;
	const bool hasVersion = xml.readProperty(document, "deasset.version", documentVersion);
	assert(hasVersion && "[MeshAsset][Assert] reason=document_version_missing");
	assert(documentVersion == version && "[MeshAsset][Assert] reason=document_version_mismatch");

	source.clear();
	xml.readProperty(document, "deasset.source", source);
}

void MeshAsset::serialize(OutputFileStream& fileStream) const
{
	const uint32 lodCount = static_cast<uint32>(meshes.size());
	fileStream << lodCount;

	for (uint32 i = 0; i < lodCount; ++i)
	{
		const RawMeshData& rawMeshData = meshes[i];
		assert(rawMeshData.isValid() && "[MeshAsset][Assert] reason=serialize_raw_mesh_data_invalid");
		rawMeshData.serialize(fileStream);
	}
}

void MeshAsset::deserialize(InputFileStream& fileStream)
{
	meshes.clear();

	uint32 lodCount = 0;
	fileStream >> lodCount;

	meshes.resize(lodCount);
	for (uint32 lodIndex = 0; lodIndex < lodCount; ++lodIndex)
	{
		meshes[lodIndex].deserialize(fileStream);
	}
}
