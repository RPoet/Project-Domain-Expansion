#pragma once

#include "Engine/Platform/PlatformDefine.h"

bool frameworkFileSystemResolveResourcesRootPath(string& outResourcesRootPath);
bool frameworkFileSystemResolveDefaultWorldFilePath(string& outWorldPath);
bool frameworkFileSystemResolveEditorWorldTemplateFilePath(string& outWorldPath);
bool frameworkFileSystemResolvePathFromResources(const string& pathText, string& outAbsolutePath);
bool frameworkFileSystemEnsureParentDirectory(const string& filePath);
string frameworkFileSystemSanitizeFileName(const string& fileNameText, const string& fallbackName = "NewWorld");
bool frameworkFileSystemResolveUniqueFilePath(
	const string& directoryPath,
	const string& fileStem,
	const string& extensionWithDot,
	string& outFilePath);
