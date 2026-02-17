#include "Engine/Framework/Framework.h"
#include "Engine/Module/Timer/Timer.h"
#include "Render/RenderCommand.h"

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
	renderedBackendFrameCount = 0;
	backendResizeCount = 0;
	backendResizeFailed = false;
	backendCreated = false;
	backendForcedResizeSubmitted = false;
	backendFinalizePending = false;
	executionCompleted = false;
	runtimeExitCode = 0;
	renderer.setBackend(nullptr);
	screen.shutdown();

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
			runtimeExitCode = 3;
		}
		else
		{
			runtimeExitCode = 6;
		}
		executionCompleted = true;
		return false;
	}

	if (executionFlow == FrameworkExecutionFlow::testFlow)
	{
		registerTest();
		return true;
	}

	if (executionFlow != FrameworkExecutionFlow::backendFlow)
	{
		return true;
	}

	return initializeBackendFlow();
}

void Framework::shutdown()
{
	shutdownModules();

	screen.shutdown();
	renderer.setBackend(nullptr);
	backendCreated = false;
	backendResizeFailed = false;
	backendForcedResizeSubmitted = false;
	backendFinalizePending = false;
	backendResizeCount = 0;
	renderedBackendFrameCount = 0;
	executionCompleted = false;
	runtimeExitCode = 0;
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
			runtimeExitCode = 1;
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

	RenderCommand::get().enqueue("Render",
		[](string&& funcName, RenderBackend& backend)
		{
			Renderer renderer;
			renderer.setBackend(&backend);
			renderer.render(backend.acquireCommandList());
		});

	RenderCommand::get().enqueue("Present",
		[](string&& funcName, RenderBackend& backend)
		{
			Screen screen;
			// do screen specific things.
		});

	return true;
}

bool Framework::isExecutionCompleted() const
{
	return executionCompleted;
}

int32 Framework::getRuntimeExitCode() const
{
	return runtimeExitCode;
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

	if (
		executionFlow == FrameworkExecutionFlow::backendFlow &&
		backendFinalizePending &&
		!executionCompleted)
	{
		backendFinalizePending = false;
		finalizeBackendFlow(true);
	}
}

bool Framework::isValidWorldIndex(const uint32 worldIndex) const
{
	return worldIndex < static_cast<uint32>(worldStorage.size());
}
