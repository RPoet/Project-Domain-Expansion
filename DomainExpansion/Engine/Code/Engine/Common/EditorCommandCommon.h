#pragma once

#include "Engine/Platform/PlatformDefine.h"

class World;

bool editorCommandParseUnsignedInteger(const string& textValue, uint32& parsedValue);
bool editorCommandParseFloat(const string& textValue, float& parsedValue);
bool editorCommandParseBoolean(const string& textValue, bool& parsedValue);

float editorCommandClampFieldOfViewYDegrees(float fieldOfViewYDegrees);
void editorCommandClampPlanes(float& nearPlane, float& farPlane);

bool editorCommandResolveAssetPathForComparison(const string& assetPath, string& outResolvedAssetPath);
bool editorCommandAreEquivalentAssetPaths(const string& leftAssetPath, const string& rightAssetPath);
bool editorCommandFindEntityIndexByAssetPath(const World& world, const string& assetPath, uint32& outEntityIndex);
bool editorCommandFindComponentIndexByAssetPath(const World& world, const string& assetPath, uint32& outComponentIndex);
