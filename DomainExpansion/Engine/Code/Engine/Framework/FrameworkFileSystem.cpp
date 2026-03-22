#include "Engine/Framework/FrameworkFileSystem.h"

bool frameworkFileSystemResolveResourcesRootPath(string& outResourcesRootPath)
{
	outResourcesRootPath.clear();

	error_code currentPathErrorCode;
	filesystem_path currentPath = current_path(currentPathErrorCode);
	if (currentPathErrorCode)
	{
		return false;
	}

	for (uint32 pathDepth = 0; pathDepth < 16; ++pathDepth)
	{
		const filesystem_path candidatePath = currentPath / "Engine" / "Resources";
		error_code candidateErrorCode;
		if (exists(candidatePath, candidateErrorCode)
			&& is_directory(candidatePath, candidateErrorCode))
		{
			outResourcesRootPath = candidatePath.lexically_normal().string();
			return true;
		}

		const filesystem_path parentPath = currentPath.parent_path();
		if (parentPath.empty() || parentPath == currentPath)
		{
			break;
		}

		currentPath = parentPath;
	}

	return false;
}

bool frameworkFileSystemResolveDefaultWorldFilePath(string& outWorldPath)
{
	outWorldPath.clear();

	string resourcesRootPath = {};
	if (!frameworkFileSystemResolveResourcesRootPath(resourcesRootPath))
	{
		return false;
	}

	outWorldPath = (filesystem_path(resourcesRootPath) / "Scenes" / "SphereTest.world")
		.lexically_normal()
		.string();
	return true;
}

bool frameworkFileSystemResolveEditorWorldTemplateFilePath(string& outWorldPath)
{
	outWorldPath.clear();

	string resourcesRootPath = {};
	if (!frameworkFileSystemResolveResourcesRootPath(resourcesRootPath))
	{
		return false;
	}

	outWorldPath = (filesystem_path(resourcesRootPath) / "Scenes" / "EditorWorldTemplate.world")
		.lexically_normal()
		.string();
	return true;
}

string frameworkFileSystemSanitizeFileName(const string& fileNameText, const string& fallbackName)
{
	string sanitizedText = fileNameText;
	for (size_t characterIndex = 0; characterIndex < sanitizedText.length(); ++characterIndex)
	{
		const char character = sanitizedText[characterIndex];
		const bool validCharacter =
			(character >= 'a' && character <= 'z')
			|| (character >= 'A' && character <= 'Z')
			|| (character >= '0' && character <= '9')
			|| character == '_'
			|| character == '-';
		if (!validCharacter)
		{
			sanitizedText[characterIndex] = '_';
		}
	}

	if (!sanitizedText.empty())
	{
		return sanitizedText;
	}

	return fallbackName.empty() ? "NewWorld" : fallbackName;
}

bool frameworkFileSystemResolveUniqueFilePath(
	const string& directoryPath,
	const string& fileStem,
	const string& extensionWithDot,
	string& outFilePath)
{
	outFilePath.clear();
	error_code createDirectoryError;
	create_directories(directoryPath, createDirectoryError);
	if (createDirectoryError)
	{
		return false;
	}

	const string resolvedExtension = extensionWithDot.empty() ? ".world" : extensionWithDot;
	const string sanitizedStem = frameworkFileSystemSanitizeFileName(fileStem, "NewWorld");

	filesystem_path candidatePath = filesystem_path(directoryPath) / (sanitizedStem + resolvedExtension);
	for (uint32 duplicateIndex = 1; exists(candidatePath) && duplicateIndex < 10000; ++duplicateIndex)
	{
		candidatePath = filesystem_path(directoryPath) / (sanitizedStem + "_" + std::to_string(duplicateIndex) + resolvedExtension);
	}

	outFilePath = candidatePath.lexically_normal().string();
	return true;
}
