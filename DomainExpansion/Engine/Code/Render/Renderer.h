#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Engine/Platform/SIMDMath.h"
#include "Render/Backends/RenderBackend.h"

struct RenderWorldDrawPrepareResult;

class Renderer
{
public:
	void setBackend(RenderBackend* renderBackend);
	void drawGeometry(
		CommandList* commandList,
		const RenderWorldDrawPrepareResult& drawPrepareResult,
		const float4x4& viewProjectionMatrix);
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
