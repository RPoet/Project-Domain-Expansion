#include "Render/Screen.h"

bool Screen::initialize(RenderBackend& renderBackend)
{
	shutdown();
	swapChain = renderBackend.getSwapChain();
	return swapChain != nullptr;
}

void Screen::shutdown()
{
	swapChain = nullptr;
}

bool Screen::isRenderable() const
{
	if (swapChain == nullptr)
	{
		return false;
	}

	return swapChain->isRenderable();
}

void Screen::present(ResourceObject* outputResource)
{
	if (swapChain == nullptr
		|| outputResource == nullptr)
	{
		return;
	}

	if (!swapChain->isRenderable())
	{
		return;
	}

	TextureResourceObject* currentBackBufferResource = swapChain->getCurrentBackBufferResource();
	if (currentBackBufferResource != outputResource)
	{
		return;
	}

	swapChain->present();
}
