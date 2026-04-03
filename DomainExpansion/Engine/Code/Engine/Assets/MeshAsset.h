#pragma once
#include "Asset.h"

using PositionData = float3;
using NormalData = float3;
using TexcoordData = float2;

struct RawMeshData
{
	vector<PositionData> positionVertices = {};
	vector<NormalData> normalVertices = {};
	vector<TexcoordData> texcoordVertices = {};
	vector<uint32> indices = {};

	void empty();
	void serialize(OutputFileStream& fileStream) const;
	void deserialize(InputFileStream& fileStream);
	bool isValid() const;
};

struct MeshSectionRange
{
	uint32 startIndex = 0;
	uint32 indexCount = 0;
};

class MeshAsset : public Asset
{
public:
	DECLARE_ASSET(MeshAsset);
	constexpr static uint32 version = 3;

	MeshAsset()
	{
		hasBinary = true;
	}

	void empty();

	vector<RawMeshData>& getMeshes();
	const vector<RawMeshData>& getMeshes() const;
	void ensureLODCount(const uint32 lodCount);
	uint32 getLODCount() const;
	RawMeshData& getRawMeshData(const uint32 lodLevel = 0);
	const RawMeshData& getRawMeshData(const uint32 lodLevel = 0) const;
	vector<MeshSectionRange>& getSectionRanges(const uint32 lodLevel = 0);
	const vector<MeshSectionRange>& getSectionRanges(const uint32 lodLevel = 0) const;
	void addSectionRange(uint32 lodLevel, uint32 startIndex, uint32 indexCount);
	void ensureSectionRanges();
	uint32 getVertexCount(const uint32 lodLevel = 0) const;
	uint32 getIndexCount(const uint32 lodLevel = 0) const;
	const string& getSource() const;
	void setSource(const string& inSource);

	void serialize(OutputFileStream& fileStream) const override;
	void deserialize(InputFileStream& fileStream) override;

private:
	void writeAssetProperty(OutputFileStream& fileStream) const override;
	void readAssetProperty(const XMLKeyValueDocument& document) override;

	// TO DO : no CPU data is not required for this, directly load mesh data into GPU.
	vector<RawMeshData> meshes;
	vector<vector<MeshSectionRange>> sectionRangesByLOD;
	string source;
};

/*
struct GPUMeshHandle
{
	uint32 positionBufferIdentifier = 0;
	uint32 normalBufferIdentifier = 0;
	uint32 texcoordBufferIdentifier = 0;
	uint32 indexBufferIdentifier = 0;
};
 
struct MeshObject
{
	string name = {};
	vector<GPUMeshHandle> meshes;
};
*/
