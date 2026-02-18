#include "Engine/Framework/Framework.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Module/Timer/Timer.h"
#include "Render/RenderCommand.h"
#include "Render/Renderer.h"
#include "Render/Screen.h"

Framework::Framework(const FrameworkExecutionFlow executionFlow)
	: executionFlow(executionFlow)
{
}

bool Framework::initialize(
	WindowsWindowObject& windowsWindowObject,
	const FrameworkInitializeOptions& initializeOptions)
{
	shutdown();

	executionFlow = initializeOptions.executionFlow;
	backendOptions = initializeOptions.backendOptions;
	this->windowsWindowObject = &windowsWindowObject;
	backendCreated = false;
	executionCompleted = false;
	runtimeExitCode = FrameworkRuntimeExitCode::success;

	WindowEventCallbacks windowEventCallbacks = {};
	windowEventCallbacks.onResize = [this](const uint32 width, const uint32 height)
	{
		onWindowResize(width, height);
	};
	windowEventCallbacks.onActivationChanged = [](const bool isActive)
	{
		output << "Window activation changed: " << (isActive ? "active" : "inactive") << lineBreak;
	};
	windowsWindowObject.setEventCallbacks(moveValue(windowEventCallbacks));

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

	backendCreated = true;
	return true;
}

void Framework::shutdown()
{
	shutdownModules();

	backendCreated = false;
	executionCompleted = false;
	runtimeExitCode = FrameworkRuntimeExitCode::success;
	windowsWindowObject = nullptr;
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
	}

	return true;
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

	updateModules();

	if (executionFlow == FrameworkExecutionFlow::testFlow)
	{
		const bool tickResult = testFramework.tick(*this);
		if (!tickResult)
		{
			runtimeExitCode = FrameworkRuntimeExitCode::testFlowTickFailed;
			executionCompleted = true;
			return false;
		}

		if (testFramework.isCompleted())
		{
			finalizeTestFlow();
		}
		return true;
	}

	float deltaTimeSeconds = static_cast<float>(Timer::get()->getDeltaTime());

	if (executionFlow == FrameworkExecutionFlow::backendFlow)
	{
		return tickBackendFlow(deltaTimeSeconds);
	}

	World* activeWorldObject = getActiveWorld();
	if (activeWorldObject == nullptr)
	{
		return false;
	}

	activeWorldObject->tick(deltaTimeSeconds);

	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	if (backendCreated
		&& windowsWindowObject != nullptr
		&& renderBackendModule != nullptr
		&& renderBackendModule->isBackendCreated()
		&& !windowsWindowObject->isWindowMinimized())
	{
		RenderBackend* renderBackend = renderBackendModule->getBackend();
		if (renderBackend != nullptr)
		{
			RenderCommand::get().enqueue("Render", [](string&& commandName, RenderBackend& renderBackendReference)
			{
				unused(commandName);
				CommandList* commandList = renderBackendReference.acquireCommandList();
				if (commandList == nullptr)
				{
					return;
				}

				Renderer renderer;
				Screen screen;
				renderer.setBackend(&renderBackendReference);

				if (!screen.initialize(renderBackendReference))
				{
					renderBackendReference.releaseCommandList(commandList);
					return;
				}

				renderer.render(commandList);

				ResourceObject* outputResource = renderer.getOutputResource();
				screen.present(outputResource);

				SyncObject* syncObject = renderBackendReference.getSyncObject();
				if (syncObject != nullptr && outputResource != nullptr)
				{
					syncObject->signal();
				}

				screen.shutdown();
				renderBackendReference.releaseCommandList(commandList);
			});
		}
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

void Framework::flushRenderCommandQueue()
{
	RenderCommand::get().flush();

	if (executionFlow == FrameworkExecutionFlow::backendFlow
		&& backendTestState.finalizePending
		&& !executionCompleted)
	{
		backendTestState.finalizePending = false;
		finalizeBackendFlow(true);
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
