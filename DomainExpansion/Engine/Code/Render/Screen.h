#pragma once

#include "Render/Backends/RenderBackend.h"

class Screen
{
public:
	bool initialize(RenderBackend& renderBackend);
	void shutdown();

	bool isRenderable() const;
	ResourceObject* getCurrentBackBufferResource();
	RenderTargetView* getCurrentBackBufferView();
	void present();

private:
	RenderBackend* renderBackend = nullptr;
	SwapChain* swapChain = nullptr;
};
