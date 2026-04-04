#pragma once

#include "Engine/Platform/PlatformDefine.h"

using FbxObjectIdentifier = long long;

struct FbxLayerElementFloat3
{
	string mappingInformationType = {};
	string referenceInformationType = {};
	vector<double> directValues = {};
	vector<int32> indices = {};
};

struct FbxLayerElementFloat2
{
	string name = {};
	string mappingInformationType = {};
	string referenceInformationType = {};
	vector<double> directValues = {};
	vector<int32> indices = {};
};

struct FbxLayerElementMaterial
{
	string mappingInformationType = {};
	string referenceInformationType = {};
	vector<int32> materialIndices = {};
};

struct FbxGeometryData
{
	string name = {};
	vector<double> vertices = {};
	vector<int32> polygonVertexIndices = {};
	FbxLayerElementFloat3 normals = {};
	vector<FbxLayerElementFloat2> uvLayers = {};
	FbxLayerElementMaterial materials = {};
};

struct FbxModelData
{
	string name = {};
	string modelType = {};
	float3 translation = {};
	float3 rotationInDegrees = {};
	float3 scaling = { 1.0f, 1.0f, 1.0f };
	FbxObjectIdentifier parentModelIdentifier = 0;
};

struct FbxMaterialData
{
	string name = {};
	string materialType = {};
};

struct FbxConnectionData
{
	string connectionType = {};
	FbxObjectIdentifier childIdentifier = 0;
	FbxObjectIdentifier parentIdentifier = 0;
	string propertyName = {};
};

struct FbxSceneData
{
	unordered_map<FbxObjectIdentifier, FbxGeometryData> geometryByIdentifier = {};
	unordered_map<FbxObjectIdentifier, FbxModelData> modelByIdentifier = {};
	unordered_map<FbxObjectIdentifier, FbxMaterialData> materialByIdentifier = {};
	vector<FbxConnectionData> connections = {};
	unordered_map<FbxObjectIdentifier, vector<FbxObjectIdentifier>> geometryToModelIdentifiers = {};
	unordered_map<FbxObjectIdentifier, vector<FbxObjectIdentifier>> modelToGeometryIdentifiers = {};
	unordered_map<FbxObjectIdentifier, vector<FbxObjectIdentifier>> modelToMaterialIdentifiers = {};
};

bool parseFbxSceneData(
	const string& meshFilePath,
	FbxSceneData& outSceneData,
	string& outErrorText);
