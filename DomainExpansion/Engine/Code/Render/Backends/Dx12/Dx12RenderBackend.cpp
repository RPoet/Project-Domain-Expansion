#include "Render/Backends/Dx12/Dx12RenderBackend.h"
#include "Render/Backends/Dx12/Dx12CommandList.h"
#include "Render/Backends/Dx12/Dx12CommandQueue.h"
#include "Render/Backends/Dx12/Dx12RenderTargetView.h"
#include "Render/Backends/Dx12/Dx12ResourceObject.h"
#include "Render/Backends/Dx12/Dx12SwapChain.h"
#include "Render/Backends/Dx12/Dx12SyncObject.h"

Dx12RenderBackend::Dx12RenderBackend()
{
	commandQueue.reset(new Dx12CommandQueue());
	swapChain.reset(new Dx12SwapChain());
	syncObject.reset(new Dx12SyncObject());

	graphicsCommandListPool.reserve(graphicsCommandListPoolCapacity);
	graphicsCommandListInUse.reserve(graphicsCommandListPoolCapacity);
	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListPoolCapacity; ++commandListIndex)
	{
		graphicsCommandListPool.push_back(unique_pointer<Dx12CommandList>(new Dx12CommandList()));
		graphicsCommandListInUse.push_back(false);
	}
}

bool Dx12RenderBackend::resize(const uint32 width, const uint32 height)
{
	if (!isCreated())
	{
		return false;
	}

	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(swapChain.get());
	if (dx12SwapChain == nullptr)
	{
		return false;
	}

	if (!waitForGpuIdle())
	{
		return false;
	}

	destroyBackBufferViewHeap();
	if (!dx12SwapChain->resize(width, height))
	{
		return false;
	}

	return createBackBufferViewHeap();
}

CommandList* Dx12RenderBackend::acquireCommandList()
{
	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListPool.size(); ++commandListIndex)
	{
		if (graphicsCommandListInUse[commandListIndex])
		{
			continue;
		}

		graphicsCommandListInUse[commandListIndex] = true;
		return graphicsCommandListPool[commandListIndex].get();
	}

	return nullptr;
}

void Dx12RenderBackend::releaseCommandList(CommandList* commandList)
{
	if (commandList == nullptr)
	{
		return;
	}

	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListPool.size(); ++commandListIndex)
	{
		if (graphicsCommandListPool[commandListIndex].get() != commandList)
		{
			continue;
		}

		graphicsCommandListInUse[commandListIndex] = false;
		return;
	}
}

CommandQueue* Dx12RenderBackend::getCommandQueue()
{
	return commandQueue.get();
}

SwapChain* Dx12RenderBackend::getSwapChain()
{
	return swapChain.get();
}

SyncObject* Dx12RenderBackend::getSyncObject()
{
	return syncObject.get();
}

RenderTargetView* Dx12RenderBackend::createRenderTargetView(ResourceObject* resourceObject)
{
	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(swapChain.get());
	Dx12ResourceObject* dx12ResourceObject = static_cast<Dx12ResourceObject*>(resourceObject);
	if (dx12SwapChain == nullptr
		|| dx12ResourceObject == nullptr
		|| dx12ResourceObject->resource == nullptr
		|| device == nullptr
		|| backBufferViewHeap == nullptr
		|| backBufferViewDescriptorSize == 0)
	{
		return nullptr;
	}

	const uint32 imageIndex = dx12SwapChain->getCurrentImageIndex();
	if (imageIndex >= frameBufferCount)
	{
		return nullptr;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE descriptorHandle = backBufferViewHeap->GetCPUDescriptorHandleForHeapStart();
	descriptorHandle.ptr += static_cast<SIZE_T>(backBufferViewDescriptorSize) * imageIndex;
	device->CreateRenderTargetView(dx12ResourceObject->resource.Get(), nullptr, descriptorHandle);

	unique_pointer<Dx12RenderTargetView> backBufferView(new Dx12RenderTargetView());
	backBufferView->descriptorHandle = descriptorHandle;
	return backBufferView.release();
}

void Dx12RenderBackend::destroyRenderTargetView(RenderTargetView* renderTargetView)
{
	delete renderTargetView;
}

HandleWindow Dx12RenderBackend::getWindowHandle() const
{
	return windowHandle;
}

uint32 Dx12RenderBackend::getBackBufferWidth() const
{
	const Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(swapChain.get());
	if (dx12SwapChain == nullptr)
	{
		return 0;
	}

	return dx12SwapChain->getBackBufferWidth();
}

uint32 Dx12RenderBackend::getBackBufferHeight() const
{
	const Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(swapChain.get());
	if (dx12SwapChain == nullptr)
	{
		return 0;
	}

	return dx12SwapChain->getBackBufferHeight();
}

uint32 Dx12RenderBackend::getBackBufferCount() const
{
	return frameBufferCount;
}

void* Dx12RenderBackend::getNativeGraphicsDevice()
{
	return device.Get();
}

void* Dx12RenderBackend::getNativeGraphicsFactory()
{
	return dxgiFactory.Get();
}

void* Dx12RenderBackend::getNativeGraphicsCommandQueue()
{
	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(commandQueue.get());
	if (dx12CommandQueue == nullptr)
	{
		return nullptr;
	}

	return dx12CommandQueue->getNativeCommandQueue();
}

bool Dx12RenderBackend::createDevice()
{
	const RenderBackendCreateOptions& createOptions = getCreateOptions();
	windowHandle = createOptions.windowHandle;
	if (windowHandle == nullptr)
	{
		return false;
	}

	if (!createFactory(createOptions.enableDebugLayer))
	{
		return false;
	}

	com_pointer<IDXGIAdapter1> selectedAdapter;
	for (uint32 adapterIndex = 0;; ++adapterIndex)
	{
		com_pointer<IDXGIAdapter1> adapter;
		if (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) == DXGI_ERROR_NOT_FOUND)
		{
			break;
		}

		DXGI_ADAPTER_DESC1 adapterDescription = {};
		adapter->GetDesc1(&adapterDescription);
		if ((adapterDescription.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
		{
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
		{
			selectedAdapter = adapter;
			break;
		}
	}

	if (selectedAdapter != nullptr)
	{
		return SUCCEEDED(D3D12CreateDevice(
			selectedAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&device)));
	}

	return SUCCEEDED(D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&device)));
}

bool Dx12RenderBackend::createCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC queueDescription = {};
	queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDescription.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queueDescription.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDescription.NodeMask = 0;

	com_pointer<ID3D12CommandQueue> createdCommandQueue;
	if (FAILED(device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&createdCommandQueue))))
	{
		return false;
	}

	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(commandQueue.get());
	if (dx12CommandQueue == nullptr)
	{
		return false;
	}

	dx12CommandQueue->setNativeCommandQueue(createdCommandQueue);
	return true;
}

bool Dx12RenderBackend::createSwapChain(uint32 width, uint32 height)
{
	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(swapChain.get());
	if (dx12SwapChain == nullptr)
	{
		return false;
	}

	return dx12SwapChain->initialize(*this, width, height);
}

bool Dx12RenderBackend::createSyncObject()
{
	Dx12SyncObject* dx12SyncObject = static_cast<Dx12SyncObject*>(syncObject.get());
	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(commandQueue.get());
	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(swapChain.get());
	if (dx12SyncObject == nullptr || dx12CommandQueue == nullptr)
	{
		return false;
	}

	return dx12SyncObject->initialize(
		device,
		dx12CommandQueue,
		dx12SwapChain,
		frameBufferCount);
}

bool Dx12RenderBackend::createBackendResources()
{
	if (!createBackBufferViewHeap())
	{
		return false;
	}

	return createCommandResources();
}

void Dx12RenderBackend::destroyBackendResources()
{
	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListPool.size(); ++commandListIndex)
	{
		Dx12CommandList* dx12CommandList = graphicsCommandListPool[commandListIndex].get();
		if (dx12CommandList != nullptr)
		{
			dx12CommandList->shutdown();
		}
	}

	resetCommandListPoolUsage();
	destroyBackBufferViewHeap();
}

void Dx12RenderBackend::destroySyncObject()
{
	Dx12SyncObject* dx12SyncObject = static_cast<Dx12SyncObject*>(syncObject.get());
	if (dx12SyncObject != nullptr)
	{
		dx12SyncObject->shutdown();
	}
}

void Dx12RenderBackend::destroySwapChain()
{
	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(swapChain.get());
	if (dx12SwapChain != nullptr)
	{
		dx12SwapChain->shutdown();
	}
}

void Dx12RenderBackend::destroyCommandQueue()
{
	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(commandQueue.get());
	if (dx12CommandQueue != nullptr)
	{
		dx12CommandQueue->setNativeCommandQueue(nullptr);
	}
}

void Dx12RenderBackend::destroyDevice()
{
	device.Reset();
	dxgiFactory.Reset();
	windowHandle = nullptr;
}

void Dx12RenderBackend::beforeDestroy()
{
	if (!isCreated())
	{
		return;
	}

	waitForGpuIdle();
}

bool Dx12RenderBackend::createFactory(const bool enableDebugLayer)
{
	uint32 factoryFlags = 0;
	if (enableDebugLayer)
	{
		com_pointer<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}

	return SUCCEEDED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&dxgiFactory)));
}

bool Dx12RenderBackend::createBackBufferViewHeap()
{
	destroyBackBufferViewHeap();

	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(swapChain.get());
	if (device == nullptr || dx12SwapChain == nullptr)
	{
		return false;
	}

	if (!dx12SwapChain->isRenderable())
	{
		return true;
	}

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDescription = {};
	descriptorHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptorHeapDescription.NumDescriptors = frameBufferCount;
	descriptorHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descriptorHeapDescription.NodeMask = 0;
	if (FAILED(device->CreateDescriptorHeap(&descriptorHeapDescription, IID_PPV_ARGS(&backBufferViewHeap))))
	{
		return false;
	}

	backBufferViewDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	return true;
}

bool Dx12RenderBackend::createCommandResources()
{
	CommandListInitializeOptions initializeOptions = {};
	initializeOptions.nativeGraphicsDevice = device.Get();
	initializeOptions.commandListType = CommandListType::graphics;

	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListPool.size(); ++commandListIndex)
	{
		Dx12CommandList* dx12CommandList = graphicsCommandListPool[commandListIndex].get();
		if (dx12CommandList == nullptr)
		{
			return false;
		}

		if (!dx12CommandList->initialize(initializeOptions))
		{
			return false;
		}
	}

	resetCommandListPoolUsage();
	return true;
}

void Dx12RenderBackend::destroyBackBufferViewHeap()
{
	backBufferViewHeap.Reset();
	backBufferViewDescriptorSize = 0;
}

bool Dx12RenderBackend::waitForGpuIdle()
{
	Dx12SyncObject* dx12SyncObject = static_cast<Dx12SyncObject*>(syncObject.get());
	if (dx12SyncObject == nullptr)
	{
		return true;
	}

	return dx12SyncObject->waitForGpuIdle();
}

void Dx12RenderBackend::resetCommandListPoolUsage()
{
	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListInUse.size(); ++commandListIndex)
	{
		graphicsCommandListInUse[commandListIndex] = false;
	}
}
