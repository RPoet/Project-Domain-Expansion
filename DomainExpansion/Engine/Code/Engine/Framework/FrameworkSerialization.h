#pragma once

#include "Engine/Platform/PlatformDefine.h"

class World;

bool frameworkSerializationLoadWorldFromFile(
	const string& worldFilePath,
	unique_pointer<World>& outWorld,
	string& outErrorText);

bool frameworkSerializationSaveWorldToFile(
	const World& world,
	const string& worldFilePath,
	string& outErrorText);
