#pragma once

#include "Engine/Framework/Framework.h"

class FrameworkTest : public Framework
{
public:
	struct InitializeOptions
	{
		bool bootstrapWorld = false;
	};

	bool initialize(
		WindowsWindowObject& windowsWindowObject,
		const InitializeOptions& initializeOptions = {});
};
