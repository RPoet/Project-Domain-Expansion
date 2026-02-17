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

	SyncObject* syncObject = renderBackend->getPrimarySyncObject();
	CommandQueue* commandQueue = renderBackend->getPrimaryCommandQueue();
	SwapChain* swapChain = renderBackend->getPrimarySwapChain();
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
	RenderTargetView* backBufferView = swapChain->getCurrentBackBufferView();
	if (backBufferResource == nullptr || backBufferView == nullptr)
	{
		return;
	}

	commandList->beginRecord();
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
	commandList->flush();
	commandQueue->execute(commandList);
}
