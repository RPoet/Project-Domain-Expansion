#include "Render/Renderer.h"

#include "Render/RenderCommand.h"

void Renderer::flushRenderCommandQueue(const RenderCommandFlushInput& flushInput)
{
	if (flushInput.clearOnly)
	{
		RenderCommand::get().clear();
		return;
	}

	RenderCommand::get().flush();
	if (flushInput.validateAfterFlush
		&& flushInput.processBackendValidationFailFast
		&& flushInput.processBackendValidationFailFast())
	{
		return;
	}

	if (flushInput.onFlushed)
	{
		flushInput.onFlushed();
	}
}

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

	ViewportArea viewportArea = {};
	viewportArea.width = static_cast<float>(swapChain->getWidth());
	viewportArea.height = static_cast<float>(swapChain->getHeight());
	commandList->setViewport(viewportArea);

	ScissorRectArea scissorRectArea = {};
	scissorRectArea.right = static_cast<int32>(swapChain->getWidth());
	scissorRectArea.bottom = static_cast<int32>(swapChain->getHeight());
	commandList->setScissorRect(scissorRectArea);

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
