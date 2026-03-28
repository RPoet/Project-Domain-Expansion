#include "Engine/Common/EditorCommandCommon.h"

#include "Engine/Framework/World.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"

#include <algorithm>
#include <cstdlib>

bool editorCommandParseUnsignedInteger(const string& textValue, uint32& parsedValue)
{
	parsedValue = 0;
	if (textValue.empty())
	{
		return false;
	}

	uint64 value = 0;
	for (size_t characterIndex = 0; characterIndex < textValue.length(); ++characterIndex)
	{
		const char character = textValue[characterIndex];
		if (character < '0' || character > '9')
		{
			return false;
		}

		value = (value * 10ull) + static_cast<uint64>(character - '0');
		if (value > static_cast<uint64>(uint32MaxValue))
		{
			return false;
		}
	}

	parsedValue = static_cast<uint32>(value);
	return true;
}

bool editorCommandParseFloat(const string& textValue, float& parsedValue)
{
	parsedValue = 0.0f;
	if (textValue.empty())
	{
		return false;
	}

	char* endPointer = nullptr;
	const float value = std::strtof(textValue.c_str(), &endPointer);
	if (endPointer == nullptr || endPointer != textValue.c_str() + textValue.length())
	{
		return false;
	}

	parsedValue = value;
	return true;
}

bool editorCommandParseBoolean(const string& textValue, bool& parsedValue)
{
	if (textValue == "1" || textValue == "true" || textValue == "TRUE")
	{
		parsedValue = true;
		return true;
	}

	if (textValue == "0" || textValue == "false" || textValue == "FALSE")
	{
		parsedValue = false;
		return true;
	}

	return false;
}

float editorCommandClampFieldOfViewYDegrees(const float fieldOfViewYDegrees)
{
	return std::clamp(fieldOfViewYDegrees, 1.0f, 179.0f);
}

void editorCommandClampPlanes(float& nearPlane, float& farPlane)
{
	nearPlane = std::max(nearPlane, 0.001f);
	farPlane = std::max(farPlane, nearPlane + 0.001f);
}

bool editorCommandResolveAssetPathForComparison(const string& assetPath, string& outResolvedAssetPath)
{
	outResolvedAssetPath.clear();
	if (assetPath.empty())
	{
		return true;
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	if (diskLoaderModule != nullptr && diskLoaderModule->resolveAbsolutePathFromResources(assetPath, outResolvedAssetPath))
	{
		return true;
	}

	outResolvedAssetPath = filesystem_path(assetPath).lexically_normal().string();
	return true;
}

bool editorCommandAreEquivalentAssetPaths(const string& leftAssetPath, const string& rightAssetPath)
{
	string resolvedLeftAssetPath = {};
	string resolvedRightAssetPath = {};
	editorCommandResolveAssetPathForComparison(leftAssetPath, resolvedLeftAssetPath);
	editorCommandResolveAssetPathForComparison(rightAssetPath, resolvedRightAssetPath);
	return resolvedLeftAssetPath == resolvedRightAssetPath;
}

bool editorCommandFindEntityIndexByAssetPath(const World& world, const string& assetPath, uint32& outEntityIndex)
{
	outEntityIndex = invalidEntityIndex;
	for (uint32 entityIndex = 0; entityIndex < world.getEntityCount(); ++entityIndex)
	{
		const Entity* entity = world.getEntityByIndex(entityIndex);
		if (entity == nullptr || !editorCommandAreEquivalentAssetPaths(entity->getAssetPath(), assetPath))
		{
			continue;
		}

		outEntityIndex = entityIndex;
		return true;
	}

	return false;
}

bool editorCommandFindComponentIndexByAssetPath(const World& world, const string& assetPath, uint32& outComponentIndex)
{
	outComponentIndex = invalidComponentIndex;
	for (uint32 componentIndex = 0; componentIndex < world.getComponentCount(); ++componentIndex)
	{
		const Component* component = world.getComponentByIndex(componentIndex);
		if (component == nullptr || !editorCommandAreEquivalentAssetPaths(component->getAssetPath(), assetPath))
		{
			continue;
		}

		outComponentIndex = componentIndex;
		return true;
	}

	return false;
}
