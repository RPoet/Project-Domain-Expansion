#include "Engine/Framework/FrameworkFileSystem.h"

bool frameworkFileSystemResolveResourcesRootPath(string& outResourcesRootPath)
{
	outResourcesRootPath.clear();

	std::error_code currentPathErrorCode;
	filesystem_path currentPath = std::filesystem::current_path(currentPathErrorCode);
	if (currentPathErrorCode)
	{
		return false;
	}

	for (uint32 pathDepth = 0; pathDepth < 16; ++pathDepth)
	{
		const filesystem_path candidatePath = currentPath / "Engine" / "Resources";
		std::error_code candidateErrorCode;
		if (std::filesystem::exists(candidatePath, candidateErrorCode)
			&& std::filesystem::is_directory(candidatePath, candidateErrorCode))
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

bool frameworkFileSystemResolvePathFromResources(const string& pathText, string& outAbsolutePath)
{
	outAbsolutePath.clear();
	if (pathText.empty())
	{
		return false;
	}

	const filesystem_path inputPath(pathText);
	std::error_code pathErrorCode;
	if (inputPath.is_absolute())
	{
		if (!std::filesystem::exists(inputPath, pathErrorCode))
		{
			return false;
		}

		outAbsolutePath = inputPath.lexically_normal().string();
		return true;
	}

	string resourcesRootPath = {};
	if (!frameworkFileSystemResolveResourcesRootPath(resourcesRootPath))
	{
		return false;
	}

	const filesystem_path resourcesRoot(resourcesRootPath);
	const filesystem_path resourcesRelativePath = resourcesRoot / inputPath;
	if (std::filesystem::exists(resourcesRelativePath, pathErrorCode))
	{
		outAbsolutePath = resourcesRelativePath.lexically_normal().string();
		return true;
	}

	const filesystem_path engineRelativePath = resourcesRoot.parent_path() / inputPath;
	if (std::filesystem::exists(engineRelativePath, pathErrorCode))
	{
		outAbsolutePath = engineRelativePath.lexically_normal().string();
		return true;
	}

	return false;
}

bool frameworkFileSystemEnsureParentDirectory(const string& filePath)
{
	const filesystem_path parentPath = filesystem_path(filePath).parent_path();
	if (parentPath.empty())
	{
		return true;
	}

	std::error_code createDirectoryError;
	std::filesystem::create_directories(parentPath, createDirectoryError);
	return !createDirectoryError;
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
	std::error_code createDirectoryError;
	std::filesystem::create_directories(directoryPath, createDirectoryError);
	if (createDirectoryError)
	{
		return false;
	}

	const string resolvedExtension = extensionWithDot.empty() ? ".world" : extensionWithDot;
	const string sanitizedStem = frameworkFileSystemSanitizeFileName(fileStem, "NewWorld");

	filesystem_path candidatePath = filesystem_path(directoryPath) / (sanitizedStem + resolvedExtension);
	for (uint32 duplicateIndex = 1; std::filesystem::exists(candidatePath) && duplicateIndex < 10000; ++duplicateIndex)
	{
		candidatePath = filesystem_path(directoryPath) / (sanitizedStem + "_" + std::to_string(duplicateIndex) + resolvedExtension);
	}

	outFilePath = candidatePath.lexically_normal().string();
	return true;
}
