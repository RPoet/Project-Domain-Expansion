#include "Render/Screen.h"

bool Screen::initialize(RenderBackend& renderBackend)
{
	shutdown();
	this->renderBackend = &renderBackend;
	swapChain = renderBackend.getPrimarySwapChain();
	return swapChain != nullptr;
}

void Screen::shutdown()
{
	swapChain = nullptr;
	renderBackend = nullptr;
}

bool Screen::isRenderable() const
{
	if (swapChain == nullptr)
	{
		return false;
	}

	return swapChain->isRenderable();
}

ResourceObject* Screen::getCurrentBackBufferResource()
{
	if (swapChain == nullptr)
	{
		return nullptr;
	}

	return swapChain->getCurrentBackBufferResource();
}

RenderTargetView* Screen::getCurrentBackBufferView()
{
	if (swapChain == nullptr)
	{
		return nullptr;
	}

	return swapChain->getCurrentBackBufferView();
}

void Screen::present()
{
	if (swapChain == nullptr)
	{
		return;
	}

	swapChain->present();
}
