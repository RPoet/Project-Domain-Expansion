#include "Render/Backends/Dx12/Dx12RenderBackend.h"
#include "Render/Backends/Dx12/Dx12CommandList.h"
#include "Render/Backends/Dx12/Dx12CommandQueue.h"
#include "Render/Backends/Dx12/Dx12SwapChain.h"
#include "Render/Backends/Dx12/Dx12SyncObject.h"

Dx12RenderBackend::Dx12RenderBackend()
{
	primaryCommandQueue.reset(new Dx12CommandQueue());
	primarySwapChain.reset(new Dx12SwapChain());
	primarySyncObject.reset(new Dx12SyncObject());

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

	if (!waitForGpuIdle())
	{
		return false;
	}

	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(primarySwapChain.get());
	if (dx12SwapChain == nullptr)
	{
		return false;
	}

	if (!dx12SwapChain->resize(width, height))
	{
		return false;
	}

	backBufferWidth = width;
	backBufferHeight = height;
	return true;
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

CommandQueue* Dx12RenderBackend::getPrimaryCommandQueue()
{
	return primaryCommandQueue.get();
}

SwapChain* Dx12RenderBackend::getPrimarySwapChain()
{
	return primarySwapChain.get();
}

SyncObject* Dx12RenderBackend::getPrimarySyncObject()
{
	return primarySyncObject.get();
}

HandleWindow Dx12RenderBackend::getWindowHandle() const
{
	return windowHandle;
}

uint32 Dx12RenderBackend::getBackBufferWidth() const
{
	return backBufferWidth;
}

uint32 Dx12RenderBackend::getBackBufferHeight() const
{
	return backBufferHeight;
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
	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(primaryCommandQueue.get());
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

	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(primaryCommandQueue.get());
	if (dx12CommandQueue == nullptr)
	{
		return false;
	}

	dx12CommandQueue->setNativeCommandQueue(createdCommandQueue);
	return true;
}

bool Dx12RenderBackend::createSwapChain(uint32 width, uint32 height)
{
	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(primarySwapChain.get());
	if (
		dx12SwapChain == nullptr)
	{
		return false;
	}

	backBufferWidth = width;
	backBufferHeight = height;
	return dx12SwapChain->initialize(*this);
}

bool Dx12RenderBackend::createSyncObject()
{
	Dx12SyncObject* dx12SyncObject = static_cast<Dx12SyncObject*>(primarySyncObject.get());
	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(primaryCommandQueue.get());
	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(primarySwapChain.get());
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
}

void Dx12RenderBackend::destroySyncObject()
{
	Dx12SyncObject* dx12SyncObject = static_cast<Dx12SyncObject*>(primarySyncObject.get());
	if (dx12SyncObject != nullptr)
	{
		dx12SyncObject->shutdown();
	}
}

void Dx12RenderBackend::destroySwapChain()
{
	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(primarySwapChain.get());
	if (dx12SwapChain != nullptr)
	{
		dx12SwapChain->shutdown();
	}
}

void Dx12RenderBackend::destroyCommandQueue()
{
	Dx12CommandQueue* dx12CommandQueue = static_cast<Dx12CommandQueue*>(primaryCommandQueue.get());
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
	backBufferWidth = 0;
	backBufferHeight = 0;
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

bool Dx12RenderBackend::createCommandResources()
{
	Dx12SwapChain* dx12SwapChain = static_cast<Dx12SwapChain*>(primarySwapChain.get());
	for (uint32 commandListIndex = 0; commandListIndex < graphicsCommandListPool.size(); ++commandListIndex)
	{
		Dx12CommandList* dx12CommandList = graphicsCommandListPool[commandListIndex].get();
		if (dx12CommandList == nullptr)
		{
			return false;
		}

		dx12CommandList->setSwapChain(dx12SwapChain);
		if (!dx12CommandList->initialize(device, frameBufferCount))
		{
			return false;
		}
	}

	resetCommandListPoolUsage();
	return true;
}

bool Dx12RenderBackend::waitForGpuIdle()
{
	Dx12SyncObject* dx12SyncObject = static_cast<Dx12SyncObject*>(primarySyncObject.get());
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
