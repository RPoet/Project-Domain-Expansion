#include "Engine/Framework/Framework.h"
#include "Engine/Framework/FrameworkFileSystem.h"
#include "Engine/Framework/FrameworkSerialization.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Module/Timer/Timer.h"
#include "Engine/Module/UI/ImGuiLayerModule.h"
#include "Engine/Module/Asset/MeshStreaming.h"
#include "Render/RenderCommand.h"

static const char* getFrameworkBackendTypeText(const RenderBackendType backendType)
{
	switch (backendType)
	{
	case RenderBackendType::dx12:
		return "dx12";
	case RenderBackendType::vulkan:
		return "vulkan";
	case RenderBackendType::metal:
		return "metal";
	default:
		return "unknown";
	}
}

static const char* getFrameworkExecutionFlowText(const FrameworkExecutionFlow executionFlow)
{
	switch (executionFlow)
	{
	case FrameworkExecutionFlow::worldFlow:
		return "world";
	case FrameworkExecutionFlow::backendFlow:
		return "backend";
	case FrameworkExecutionFlow::testFlow:
		return "test";
	default:
		return "unknown";
	}
}

Framework::Framework(const FrameworkExecutionFlow executionFlow)
	: executionFlow(executionFlow)
{
}

bool Framework::initialize(
	WindowsWindowObject& inWindowsWindowObject,
	const FrameworkInitializeOptions& initializeOptions)
{
	shutdown();

	executionFlow = initializeOptions.executionFlow;
	backendOptions = initializeOptions.backendOptions;
	windowsWindowObject = &inWindowsWindowObject;
	executionCompleted = false;
	runtimeExitCode = FrameworkRuntimeExitCode::success;
	activeWorldFilePath.clear();

	const bool forceEnableDebugLayer =
		executionFlow == FrameworkExecutionFlow::worldFlow
		|| executionFlow == FrameworkExecutionFlow::backendFlow;
	if (forceEnableDebugLayer && !backendOptions.enableDebugLayer)
	{
		output << "[BackendValidation][Policy] flow=" << getFrameworkExecutionFlowText(executionFlow)
			   << " backend=" << getFrameworkBackendTypeText(backendOptions.backendType)
			   << " key=force_debug_layer previous=0 current=1" << lineBreak;
	}
	if (forceEnableDebugLayer)
	{
		backendOptions.enableDebugLayer = true;
	}

	WindowEventCallbacks windowEventCallbacks = {};
	windowEventCallbacks.onResize = [this](const uint32 width, const uint32 height)
	{
		onWindowResize(width, height);
	};
	windowEventCallbacks.onActivationChanged = [](const bool isActive)
	{
		output << "Window activation changed: " << (isActive ? "active" : "inactive") << lineBreak;
	};
	windowEventCallbacks.onNativeMessage = [](
		const HandleWindow windowHandle,
		const MessageIdentifier messageIdentifier,
		const MessageFirstParameter firstParameter,
		const MessageSecondParameter secondParameter) -> bool
	{
		shared_pointer<ImGuiLayerModule> imGuiLayerModule = ImGuiLayerModule::get();
		if (imGuiLayerModule == nullptr)
		{
			return false;
		}

		return imGuiLayerModule->processNativeMessage(
			windowHandle,
			messageIdentifier,
			firstParameter,
			secondParameter);
	};
	windowsWindowObject->setEventCallbacks(moveValue(windowEventCallbacks));

	registerModule();
	if (!initializeModules())
	{
		error << "Framework module initialization failed." << lineBreak;
		if (executionFlow == FrameworkExecutionFlow::backendFlow)
		{
			runtimeExitCode = FrameworkRuntimeExitCode::backendFlowRuntimeFailure;
		}
		else
		{
			runtimeExitCode = FrameworkRuntimeExitCode::moduleInitializationFailure;
		}
		executionCompleted = true;
		return false;
	}

	if (executionFlow == FrameworkExecutionFlow::testFlow)
	{
		initializeTestFlow();
		registerTest();
		return true;
	}

	if (executionFlow == FrameworkExecutionFlow::backendFlow)
	{
		return initializeBackendFlow();
	}

	if (getActiveWorld() == nullptr)
	{
		string defaultWorldPath = {};
		if (frameworkFileSystemResolveDefaultWorldFilePath(defaultWorldPath)
			&& loadWorldFromFile(defaultWorldPath))
		{
			return true;
		}

		const uint32 editorWorldIndex = createWorld(L"EditorWorld");
		if (!loadWorld(editorWorldIndex))
		{
			error << "World bootstrap failed. reason=editor_world_load_failed" << lineBreak;
			runtimeExitCode = FrameworkRuntimeExitCode::moduleInitializationFailure;
			executionCompleted = true;
			return false;
		}
	}

	return true;
}

void Framework::shutdown()
{
	shutdownModules();

	executionCompleted = false;
	runtimeExitCode = FrameworkRuntimeExitCode::success;
	windowsWindowObject = nullptr;
	activeWorldFilePath.clear();
}

void Framework::setExecutionFlow(const FrameworkExecutionFlow executionFlow)
{
	this->executionFlow = executionFlow;
}

FrameworkExecutionFlow Framework::getExecutionFlow() const
{
	return executionFlow;
}

uint32 Framework::createWorld(const wstring& worldName)
{
	unique_pointer<World> worldInstance(new World(worldName));
	worldStorage.push_back(moveValue(worldInstance));
	return static_cast<uint32>(worldStorage.size() - 1);
}

bool Framework::loadWorld(const uint32 worldIndex)
{
	if (!isValidWorldIndex(worldIndex))
	{
		return false;
	}

	if (worldStorage[worldIndex] == nullptr)
	{
		return false;
	}

	activeWorldIndex = worldIndex;
	activeWorldFilePath.clear();
	return true;
}

bool Framework::changeWorld(const uint32 worldIndex)
{
	return loadWorld(worldIndex);
}

bool Framework::unloadWorld(const uint32 worldIndex)
{
	if (!isValidWorldIndex(worldIndex))
	{
		return false;
	}

	worldStorage[worldIndex].reset();
	if (activeWorldIndex == worldIndex)
	{
		activeWorldIndex = invalidWorldIndex;
		activeWorldFilePath.clear();
	}

	return true;
}

bool Framework::loadWorldFromFile(const string& worldFilePath)
{
	unique_pointer<World> loadedWorld = nullptr;
	string errorText = {};
	if (!frameworkSerializationLoadWorldFromFile(worldFilePath, loadedWorld, errorText)
		|| loadedWorld == nullptr)
	{
		error << "[Framework][Error] loadWorldFromFile_failed path=" << worldFilePath
			  << " reason=" << (errorText.empty() ? "unknown" : errorText) << lineBreak;
		return false;
	}

	worldStorage.push_back(moveValue(loadedWorld));
	const uint32 worldIndex = static_cast<uint32>(worldStorage.size() - 1);
	if (!loadWorld(worldIndex))
	{
		return false;
	}

	activeWorldFilePath = worldFilePath;
	return true;
}

bool Framework::saveActiveWorldToFile()
{
	const World* activeWorld = getActiveWorld();
	if (activeWorld == nullptr || activeWorldFilePath.empty())
	{
		return false;
	}

	string errorText = {};
	if (!frameworkSerializationSaveWorldToFile(*activeWorld, activeWorldFilePath, errorText))
	{
		error << "[Framework][Error] saveActiveWorldToFile_failed path=" << activeWorldFilePath
			  << " reason=" << (errorText.empty() ? "unknown" : errorText) << lineBreak;
		return false;
	}

	return true;
}

const string& Framework::getActiveWorldFilePath() const
{
	return activeWorldFilePath;
}

World* Framework::getWorld(const uint32 worldIndex)
{
	if (!isValidWorldIndex(worldIndex))
	{
		return nullptr;
	}

	return worldStorage[worldIndex].get();
}

const World* Framework::getWorld(const uint32 worldIndex) const
{
	if (!isValidWorldIndex(worldIndex))
	{
		return nullptr;
	}

	return worldStorage[worldIndex].get();
}

World* Framework::getActiveWorld()
{
	return getWorld(activeWorldIndex);
}

const World* Framework::getActiveWorld() const
{
	return getWorld(activeWorldIndex);
}

uint32 Framework::getActiveWorldIndex() const
{
	return activeWorldIndex;
}

bool Framework::update()
{
	if (executionCompleted)
	{
		return true;
	}

	if (executionFlow == FrameworkExecutionFlow::testFlow)
	{
		return updateTestExecutionFlow();
	}

	if (executionFlow == FrameworkExecutionFlow::backendFlow)
	{
		return updateBackendExecutionFlow();
	}

	preUpdateModules();
	const float deltaTimeSeconds = static_cast<float>(Timer::get()->getDeltaTime());
	World* activeWorldObject = getActiveWorld();
	if (activeWorldObject == nullptr)
	{
		return false;
	}

	activeWorldObject->tick(deltaTimeSeconds);
	postUpdateModules();

	// TO DO : consider the way to use conditional variable ? those backend call looks not correct placed in here.
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	if (renderBackendModule != nullptr && renderBackendModule->isBackendCreated()
		&& windowsWindowObject != nullptr && !windowsWindowObject->isWindowMinimized())
	{
		RenderCommand& renderCommand = RenderCommand::get();
		renderCommand.enqueue("MeshUpload", [](string&& commandName, RenderBackend& renderBackendReference)
		{
			unused(commandName);
			MeshStreaming::get()->flushGpuRequests(renderBackendReference);
		});

		renderCommand.enqueue("Render", [](string&& commandName, RenderBackend& renderBackendReference)
		{
			unused(commandName);

			SwapChain* swapChain = renderBackendReference.getSwapChain();
			SyncObject* syncObject = renderBackendReference.getSyncObject();
			if (swapChain == nullptr
				|| syncObject == nullptr)
			{
				return;
			}

			syncObject->wait();
			if (!swapChain->isRenderable())
			{
				return;
			}

			ResourceObject* outputResource = swapChain->getCurrentBackBufferResource();
			if (outputResource == nullptr)
			{
				renderBackendReference.releaseQueuedRenderResources();
				return;
			}

			RenderTargetView* renderTargetView = renderBackendReference.createRenderTargetView(outputResource);
			CommandList* commandList = renderBackendReference.acquireCommandList();
			if (renderTargetView == nullptr || commandList == nullptr)
			{
				if (commandList != nullptr)
				{
					renderBackendReference.releaseCommandList(commandList);
				}
				if (renderTargetView != nullptr)
				{
					renderBackendReference.destroyRenderTargetView(renderTargetView);
				}
				return;
			}

			commandList->reset();
			commandList->resourceBarrier(
				outputResource,
				ResourceState::present,
				ResourceState::renderTarget);
			commandList->setRenderTarget(renderTargetView);

			ViewportArea viewportArea = {};
			viewportArea.width = static_cast<float>(swapChain->getWidth());
			viewportArea.height = static_cast<float>(swapChain->getHeight());
			commandList->setViewport(viewportArea);

			ScissorRectArea scissorRectArea = {};
			scissorRectArea.right = static_cast<int32>(swapChain->getWidth());
			scissorRectArea.bottom = static_cast<int32>(swapChain->getHeight());
			commandList->setScissorRect(scissorRectArea);
			commandList->clearRenderTarget(
				renderTargetView,
				0.07f,
				0.11f,
				0.17f,
				1.0f);
			commandList->close();

			renderBackendReference.queueRenderTargetViewForDestroy(renderTargetView);
			renderBackendReference.queueCommandList(commandList);
		});

		renderCommand.enqueue("UI", [](string&& commandName, RenderBackend& renderBackendReference)
		{
			unused(commandName);

			shared_pointer<ImGuiLayerModule> imGuiLayerModule = ImGuiLayerModule::get();
			if (imGuiLayerModule == nullptr)
			{
				return;
			}

			SwapChain* swapChain = renderBackendReference.getSwapChain();
			if (swapChain == nullptr
				|| !swapChain->isRenderable())
			{
				return;
			}

			ResourceObject* outputResource = swapChain->getCurrentBackBufferResource();
			if (outputResource == nullptr)
			{
				return;
			}

			RenderTargetView* renderTargetView = renderBackendReference.createRenderTargetView(outputResource);
			CommandList* commandList = renderBackendReference.acquireCommandList();
			if (renderTargetView == nullptr || commandList == nullptr)
			{
				if (commandList != nullptr)
				{
					renderBackendReference.releaseCommandList(commandList);
				}
				if (renderTargetView != nullptr)
				{
					renderBackendReference.destroyRenderTargetView(renderTargetView);
				}
				return;
			}

			commandList->reset();
			commandList->setRenderTarget(renderTargetView);
			imGuiLayerModule->buildAndRender(commandList);
			commandList->close();

			renderBackendReference.queueRenderTargetViewForDestroy(renderTargetView);
			renderBackendReference.queueCommandList(commandList);
		});

		renderCommand.enqueue("Present", [](string&& commandName, RenderBackend& renderBackendReference)
		{
			unused(commandName);

			CommandQueue* commandQueue = renderBackendReference.getCommandQueue();
			SwapChain* swapChain = renderBackendReference.getSwapChain();
			SyncObject* syncObject = renderBackendReference.getSyncObject();
			if (commandQueue == nullptr
				|| swapChain == nullptr
				|| syncObject == nullptr
				|| !swapChain->isRenderable())
			{
				renderBackendReference.releaseQueuedRenderResources();
				return;
			}

			ResourceObject* outputResource = swapChain->getCurrentBackBufferResource();
			if (outputResource == nullptr)
			{
				return;
			}

			CommandList* commandList = renderBackendReference.acquireCommandList();
			if (commandList == nullptr)
			{
				renderBackendReference.releaseQueuedRenderResources();
				return;
			}

			commandList->reset();
			commandList->resourceBarrier(
				outputResource,
				ResourceState::renderTarget,
				ResourceState::present);
			commandList->close();

			renderBackendReference.queueCommandList(commandList);
			renderBackendReference.executeQueuedCommandLists();
			swapChain->present();
			syncObject->signal();
			renderBackendReference.releaseQueuedRenderResources();
		});
	}

	return true;
}

bool Framework::isExecutionCompleted() const
{
	return executionCompleted;
}

int32 Framework::getRuntimeExitCode() const
{
	return static_cast<int32>(runtimeExitCode);
}

const FrameworkBackendOptions& Framework::getBackendOptions() const
{
	return backendOptions;
}

WindowsWindowObject* Framework::getWindowObject()
{
	return windowsWindowObject;
}

const WindowsWindowObject* Framework::getWindowObject() const
{
	return windowsWindowObject;
}


// TO DO : Refactor this, RenderCommand must be working like it has its own world, and not included in the Framework.
void Framework::flushRenderCommandQueue()
{
	// TO DO : need No Backend version to run test flow w/o backend initialization
	if (executionFlow == FrameworkExecutionFlow::testFlow)
	{
		RenderCommand::get().clear();
		return;
	}

	RenderCommand::get().flush();

	if (!executionCompleted
		&& (executionFlow == FrameworkExecutionFlow::worldFlow
			|| executionFlow == FrameworkExecutionFlow::backendFlow))
	{
		if (processBackendValidationFailFast())
		{
			return;
		}
	}

	if (executionFlow == FrameworkExecutionFlow::backendFlow
		&& !executionCompleted)
	{
		if (backendTestState.finalizePending)
		{
			backendTestState.finalizePending = false;
			finalizeBackendFlow(true);
		}
	}
}

void Framework::initializeTestFlow()
{
	resetBackendTestState();
}

void Framework::resetBackendTestState()
{
	backendTestState.reset();
}

bool Framework::isValidWorldIndex(const uint32 worldIndex) const
{
	return worldIndex < static_cast<uint32>(worldStorage.size());
}
