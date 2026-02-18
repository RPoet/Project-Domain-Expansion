#include "Render/Renderer.h"

void Renderer::setBackend(RenderBackend* renderBackend)
{
	this->renderBackend = renderBackend;
}

void Renderer::render(CommandList* commandList)
{
	if (renderBackend == nullptr)
	{
		return;
	}

	SyncObject* syncObject = renderBackend->getSyncObject();
	CommandQueue* commandQueue = renderBackend->getCommandQueue();
	SwapChain* swapChain = renderBackend->getSwapChain();
	if (syncObject == nullptr || commandQueue == nullptr || commandList == nullptr || swapChain == nullptr)
	{
		return;
	}

	syncObject->wait();
	if (!swapChain->isRenderable())
	{
		return;
	}

	ResourceObject* backBufferResource = swapChain->getCurrentBackBufferResource();
	RenderTargetView* backBufferView = renderBackend->createRenderTargetView(backBufferResource);
	if (backBufferResource == nullptr || backBufferView == nullptr)
	{
		return;
	}

	outputResource = backBufferResource;

	commandList->reset();
	commandList->resourceBarrier(
		backBufferResource,
		ResourceState::present,
		ResourceState::renderTarget);
	commandList->setRenderTarget(backBufferView);

	commandList->clearRenderTarget(
		backBufferView,
		clearColor.red,
		clearColor.green,
		clearColor.blue,
		clearColor.alpha);

	commandList->resourceBarrier(
		backBufferResource,
		ResourceState::renderTarget,
		ResourceState::present);
	commandList->close();
	commandQueue->execute(commandList);

	renderBackend->destroyRenderTargetView(backBufferView);
}

ResourceObject* Renderer::getOutputResource() const
{
	return outputResource;
}
