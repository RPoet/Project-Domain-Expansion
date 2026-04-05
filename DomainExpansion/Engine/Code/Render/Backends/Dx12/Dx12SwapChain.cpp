#include "Render/Backends/Dx12/Dx12SwapChain.h"
#include "Render/Backends/RenderBackend.h"
#include "Render/Backends/Dx12/Dx12ResourceObject.h"

bool Dx12SwapChain::initialize(
	RenderBackend& renderBackend,
	const uint32 width,
	const uint32 height)
{
	shutdown();

	const HandleWindow windowHandle = renderBackend.getWindowHandle();
	IDXGIFactory6* nativeDxgiFactory = static_cast<IDXGIFactory6*>(renderBackend.getNativeGraphicsFactory());
	ID3D12CommandQueue* nativeCommandQueue = static_cast<ID3D12CommandQueue*>(renderBackend.getNativeGraphicsCommandQueue());

	if (windowHandle == nullptr
		|| nativeDxgiFactory == nullptr
		|| nativeCommandQueue == nullptr)
	{
		return false;
	}

	this->windowHandle = windowHandle;
	frameBufferCount = defaultFrameBufferCount;

	uint32 createWidth = width;
	if (createWidth == 0)
	{
		createWidth = 1;
	}

	uint32 createHeight = height;
	if (createHeight == 0)
	{
		createHeight = 1;
	}

	DXGI_SWAP_CHAIN_DESC1 swapChainDescription = {};
	swapChainDescription.Width = createWidth;
	swapChainDescription.Height = createHeight;
	swapChainDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDescription.Stereo = boolFalse;
	swapChainDescription.SampleDesc.Count = 1;
	swapChainDescription.SampleDesc.Quality = 0;
	swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDescription.BufferCount = frameBufferCount;
	swapChainDescription.Scaling = DXGI_SCALING_STRETCH;
	swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDescription.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	swapChainDescription.Flags = 0;

	com_pointer<IDXGISwapChain1> intermediateSwapChain;
	if (FAILED(nativeDxgiFactory->CreateSwapChainForHwnd(
		nativeCommandQueue,
		windowHandle,
		&swapChainDescription,
		nullptr,
		nullptr,
		&intermediateSwapChain)))
	{
		shutdown();
		return false;
	}

	nativeDxgiFactory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER);
	if (FAILED(intermediateSwapChain->QueryInterface(IID_PPV_ARGS(swapChain.ReleaseAndGetAddressOf()))))
	{
		shutdown();
		return false;
	}

	backBufferWidth = width;
	backBufferHeight = height;
	return createBackBufferResources();
}

bool Dx12SwapChain::resize(const uint32 width, const uint32 height)
{
	if (swapChain == nullptr)
	{
		return false;
	}

	if (backBufferWidth == width && backBufferHeight == height)
	{
		return true;
	}

	if (width == 0 || height == 0)
	{
		backBufferWidth = width;
		backBufferHeight = height;
		releaseBackBufferResources();
		return true;
	}

	releaseBackBufferResources();
	if (FAILED(swapChain->ResizeBuffers(
		frameBufferCount,
		width,
		height,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		0)))
	{
		return false;
	}

	backBufferWidth = width;
	backBufferHeight = height;
	return createBackBufferResources();
}

void Dx12SwapChain::shutdown()
{
	releaseBackBufferResources();
	swapChain.Reset();
	windowHandle = nullptr;
	frameBufferCount = 0;
	backBufferWidth = 0;
	backBufferHeight = 0;
}

TextureResourceObject* Dx12SwapChain::getBackBufferResource(const uint32 imageIndex)
{
	if (imageIndex >= backBufferResources.size())
	{
		return nullptr;
	}

	return backBufferResources[imageIndex].get();
}

TextureResourceObject* Dx12SwapChain::getCurrentBackBufferResource()
{
	if (!isRenderable())
	{
		return nullptr;
	}

	return getBackBufferResource(getCurrentImageIndex());
}

uint32 Dx12SwapChain::getFrameBufferCount() const
{
	return frameBufferCount;
}

bool Dx12SwapChain::isRenderable() const
{
	return swapChain != nullptr
		&& backBufferWidth > 0
		&& backBufferHeight > 0;
}

uint32 Dx12SwapChain::getWidth() const
{
	return backBufferWidth;
}

uint32 Dx12SwapChain::getHeight() const
{
	return backBufferHeight;
}

uint32 Dx12SwapChain::getCurrentImageIndex() const
{
	if (swapChain == nullptr)
	{
		return 0;
	}

	return static_cast<uint32>(swapChain->GetCurrentBackBufferIndex());
}

void Dx12SwapChain::present()
{
	if (!isRenderable())
	{
		return;
	}

	swapChain->Present(1, 0);
}

bool Dx12SwapChain::createBackBufferResources()
{
	if (!isRenderable())
	{
		return true;
	}

	backBufferResources.resize(frameBufferCount);
	for (uint32 frameIndex = 0; frameIndex < frameBufferCount; ++frameIndex)
	{
		unique_pointer<Dx12TextureResourceObject> backBufferResource(new Dx12TextureResourceObject());
		if (FAILED(swapChain->GetBuffer(
			frameIndex,
			IID_PPV_ARGS(&backBufferResource->getUnderlyingResource()))))
		{
			releaseBackBufferResources();
			return false;
		}

		backBufferResources[frameIndex] = moveValue(backBufferResource);
	}

	return true;
}

void Dx12SwapChain::releaseBackBufferResources()
{
	backBufferResources.clear();
}
