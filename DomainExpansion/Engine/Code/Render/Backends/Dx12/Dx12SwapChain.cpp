#include "Render/Backends/Dx12/Dx12SwapChain.h"
#include "Render/Backends/RenderBackend.h"
#include "Render/Backends/Dx12/Dx12RenderTargetView.h"
#include "Render/Backends/Dx12/Dx12ResourceObject.h"

bool Dx12SwapChain::initialize(RenderBackend& renderBackend)
{
	shutdown();

	const HandleWindow windowHandle = renderBackend.getWindowHandle();
	const uint32 width = renderBackend.getBackBufferWidth();
	const uint32 height = renderBackend.getBackBufferHeight();
	const uint32 frameBufferCount = renderBackend.getBackBufferCount();
	IDXGIFactory6* nativeDxgiFactory = static_cast<IDXGIFactory6*>(renderBackend.getNativeGraphicsFactory());
	ID3D12CommandQueue* nativeCommandQueue = static_cast<ID3D12CommandQueue*>(renderBackend.getNativeGraphicsCommandQueue());
	ID3D12Device* nativeDevice = static_cast<ID3D12Device*>(renderBackend.getNativeGraphicsDevice());

	if (
		windowHandle == nullptr ||
		nativeDxgiFactory == nullptr ||
		nativeCommandQueue == nullptr ||
		nativeDevice == nullptr ||
		frameBufferCount == 0)
	{
		return false;
	}

	this->windowHandle = windowHandle;
	this->device = nativeDevice;
	this->frameBufferCount = frameBufferCount;

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
	if (FAILED(intermediateSwapChain.As(&swapChain)))
	{
		shutdown();
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDescription = {};
	descriptorHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptorHeapDescription.NumDescriptors = frameBufferCount;
	descriptorHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descriptorHeapDescription.NodeMask = 0;
	if (FAILED(this->device->CreateDescriptorHeap(
		&descriptorHeapDescription,
		IID_PPV_ARGS(&backBufferViewHeap))))
	{
		shutdown();
		return false;
	}

	backBufferViewDescriptorSize = this->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	backBufferWidth = width;
	backBufferHeight = height;

	if (!createBackBufferResources())
	{
		shutdown();
		return false;
	}

	return true;
}

bool Dx12SwapChain::resize(const uint32 width, const uint32 height)
{
	if (swapChain == nullptr)
	{
		return false;
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
	backBufferViewHeap.Reset();
	swapChain.Reset();
	device.Reset();
	windowHandle = nullptr;
	frameBufferCount = 0;
	backBufferViewDescriptorSize = 0;
	backBufferWidth = 0;
	backBufferHeight = 0;
}

bool Dx12SwapChain::isRenderable() const
{
	return
		swapChain != nullptr &&
		backBufferWidth > 0 &&
		backBufferHeight > 0;
}

uint32 Dx12SwapChain::getCurrentImageIndex() const
{
	if (swapChain == nullptr)
	{
		return 0;
	}

	return static_cast<uint32>(swapChain->GetCurrentBackBufferIndex());
}

ResourceObject* Dx12SwapChain::getCurrentBackBufferResource()
{
	if (!isRenderable())
	{
		return nullptr;
	}

	const uint32 frameIndex = getCurrentImageIndex();
	if (frameIndex >= backBufferResources.size())
	{
		return nullptr;
	}

	return backBufferResources[frameIndex].get();
}

RenderTargetView* Dx12SwapChain::getCurrentBackBufferView()
{
	if (!isRenderable())
	{
		return nullptr;
	}

	const uint32 frameIndex = getCurrentImageIndex();
	if (frameIndex >= backBufferViews.size())
	{
		return nullptr;
	}

	return backBufferViews[frameIndex].get();
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
	if (
		device == nullptr ||
		swapChain == nullptr ||
		backBufferViewHeap == nullptr ||
		frameBufferCount == 0)
	{
		return false;
	}

	backBufferResources.resize(frameBufferCount);
	backBufferViews.resize(frameBufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = backBufferViewHeap->GetCPUDescriptorHandleForHeapStart();
	for (uint32 frameIndex = 0; frameIndex < frameBufferCount; ++frameIndex)
	{
		unique_pointer<Dx12ResourceObject> backBufferResource(new Dx12ResourceObject());
		if (FAILED(swapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&backBufferResource->resource))))
		{
			return false;
		}

		device->CreateRenderTargetView(backBufferResource->resource.Get(), nullptr, descriptorHandle);
		backBufferResources[frameIndex] = moveValue(backBufferResource);

		unique_pointer<Dx12RenderTargetView> renderTargetView(new Dx12RenderTargetView());
		renderTargetView->descriptorHandle = descriptorHandle;
		backBufferViews[frameIndex] = moveValue(renderTargetView);

		descriptorHandle.ptr += static_cast<SIZE_T>(backBufferViewDescriptorSize);
	}

	return true;
}

void Dx12SwapChain::releaseBackBufferResources()
{
	for (uint32 frameIndex = 0; frameIndex < backBufferResources.size(); ++frameIndex)
	{
		backBufferResources[frameIndex].reset();
	}

	for (uint32 frameIndex = 0; frameIndex < backBufferViews.size(); ++frameIndex)
	{
		backBufferViews[frameIndex].reset();
	}

	backBufferResources.clear();
	backBufferViews.clear();
}
