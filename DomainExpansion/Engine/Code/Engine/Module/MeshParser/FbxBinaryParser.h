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

struct FbxGeometryData
{
	string name = {};
	vector<double> vertices = {};
	vector<int32> polygonVertexIndices = {};
	FbxLayerElementFloat3 normals = {};
	vector<FbxLayerElementFloat2> uvLayers = {};
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

struct FbxSceneData
{
	unordered_map<FbxObjectIdentifier, FbxGeometryData> geometryByIdentifier = {};
	unordered_map<FbxObjectIdentifier, FbxModelData> modelByIdentifier = {};
	unordered_map<FbxObjectIdentifier, vector<FbxObjectIdentifier>> geometryToModelIdentifiers = {};
};

bool parseFbxSceneData(
	const string& meshFilePath,
	FbxSceneData& outSceneData,
	string& outErrorText);
