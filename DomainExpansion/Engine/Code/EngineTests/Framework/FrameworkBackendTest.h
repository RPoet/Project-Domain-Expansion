#pragma once

#include "Engine/Framework/Framework.h"

class FrameworkBackendTest : public Framework
{
public:
	struct InitializeOptions
	{
		bool bootstrapWorld = false;
		RenderBackendType backendType = RenderBackendType::dx12;
#if defined(_DEBUG)
		bool enableDebugLayer = true;
#else
		bool enableDebugLayer = false;
#endif
		BackendValidationInjectMode validationInjectMode = BackendValidationInjectMode::none;
	};

	bool initialize(
		WindowsWindowObject& windowsWindowObject,
		const InitializeOptions& initializeOptions = {});
};
