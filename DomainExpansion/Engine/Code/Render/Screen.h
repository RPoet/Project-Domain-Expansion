#pragma once

#include "Render/Backends/RenderBackend.h"

class Screen
{
public:
	bool initialize(RenderBackend& renderBackend);
	void shutdown();

	bool isRenderable() const;
	void present(ResourceObject* outputResource);

private:
	SwapChain* swapChain = nullptr;
};
