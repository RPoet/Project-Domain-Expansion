#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/Backends/RenderBackend.h"

class Renderer
{
public:
	void setBackend(RenderBackend* renderBackend);
	void render(CommandList* commandList);

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
};
