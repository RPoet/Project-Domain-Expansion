#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/Backends/RenderBackend.h"

class Renderer
{
public:
	struct RenderCommandFlushInput
	{
		bool clearOnly = false;
		bool validateAfterFlush = false;
		function<bool()> processBackendValidationFailFast = {};
		function<void()> onFlushed = {};
	};

	static void flushRenderCommandQueue(const RenderCommandFlushInput& flushInput);
	void setBackend(RenderBackend* renderBackend);
	void render(CommandList* commandList);
	ResourceObject* getOutputResource() const;

private:
	struct ClearColorValue
	{
		float red = 0.07f;
		float green = 0.11f;
		float blue = 0.17f;
		float alpha = 1.0f;
	};

	RenderBackend* renderBackend = nullptr;
	ClearColorValue clearColor = {};
	ResourceObject* outputResource = nullptr;
};
