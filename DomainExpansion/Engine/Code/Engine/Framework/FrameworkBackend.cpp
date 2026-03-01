#include "Engine/Framework/Framework.h"
#include "Engine/Module/Asset/MeshStreaming.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Module/Timer/Timer.h"
#include "Render/RenderCommand.h"
#include "Render/Renderer.h"
#include "Render/Screen.h"

static const char* getBackendTypeText(const RenderBackendType backendType)
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

static const char* getBackendFlowText(const FrameworkExecutionFlow executionFlow)
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

static string normalizeValidationText(const string& text)
{
	string normalizedText = {};
	normalizedText.reserve(text.size());
	for (size_t characterIndex = 0; characterIndex < text.size(); ++characterIndex)
	{
		const char character = text[characterIndex];
		if (character == '\n' || character == '\r' || character == '\t')
		{
			normalizedText.push_back(' ');
			continue;
		}

		normalizedText.push_back(character);
	}

	if (normalizedText.empty())
	{
		return "no_description";
	}

	return normalizedText;
}

bool Framework::processBackendValidationFailFast()
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	if (renderBackendModule == nullptr || !renderBackendModule->isBackendCreated())
	{
		return false;
	}

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	if (renderBackend == nullptr)
	{
		return false;
	}

	const BackendValidationPollResult pollResult = collectBackendMessages(
		backendOptions.backendType,
		renderBackend,
		backendOptions.validationInjectMode);

	const char* flowText = getBackendFlowText(executionFlow);
	const char* backendText = getBackendTypeText(backendOptions.backendType);
	for (uint32 messageIndex = 0; messageIndex < static_cast<uint32>(pollResult.messages.size()); ++messageIndex)
	{
		const BackendValidationMessage& message = pollResult.messages[messageIndex];
		const bool failingMessage = isBackendValidationFailingSeverity(message.severity);
		const string messageId = message.id.empty() ? "unknown" : message.id;
		output_stream& selectedStream = failingMessage ? error : output;
		selectedStream << "[BackendValidation][Message] flow=" << flowText
					   << " backend=" << backendText
					   << " severity=" << getBackendValidationSeverityText(message.severity)
					   << " id=" << messageId
					   << " action=" << (failingMessage ? "fail" : "log")
					   << " text=" << normalizeValidationText(message.text) << lineBreak;
	}

	const bool validationFailed = pollResult.failingCount > 0;
	output_stream& summaryStream = validationFailed ? error : output;
	summaryStream << "[BackendValidation][Summary] flow=" << flowText
				  << " backend=" << backendText
				  << " total=" << pollResult.totalCount
				  << " failing=" << pollResult.failingCount
				  << " threshold=warning"
				  << " result=" << (validationFailed ? "fail" : "pass") << lineBreak;

	if (!validationFailed)
	{
		return false;
	}

	if (executionFlow == FrameworkExecutionFlow::backendFlow)
	{
		backendTestState.validationFailed = true;
		finalizeBackendFlow(false);
		runtimeExitCode = FrameworkRuntimeExitCode::debugValidationFailure;
		return true;
	}

	if (executionFlow == FrameworkExecutionFlow::worldFlow)
	{
		executionCompleted = true;
		runtimeExitCode = FrameworkRuntimeExitCode::debugValidationFailure;
		return true;
	}

	return false;
}

bool Framework::initializeBackendFlow()
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	const bool backendCliFlow = executionFlow == FrameworkExecutionFlow::backendFlow;
	resetBackendTestState();

	if (backendCliFlow)
	{
		output << "[BackendCLI][Begin] mode=backend api="
			   << getBackendTypeText(backendOptions.backendType)
			   << " frames=" << backendOptions.frameCount << lineBreak;
	}

	if (renderBackendModule == nullptr)
	{
		if (backendCliFlow)
		{
			error << "[BackendCLI][Error] stage=create reason=backend_module_missing" << lineBreak;
		}
		else
		{
			error << "Render initialize failed. reason=backend_module_missing" << lineBreak;
		}
		runtimeExitCode = FrameworkRuntimeExitCode::backendFlowRuntimeFailure;
		executionCompleted = true;
		return false;
	}

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	if (renderBackend == nullptr)
	{
		if (backendCliFlow)
		{
			error << "[BackendCLI][Error] stage=create reason=backend_not_initialized_by_module" << lineBreak;
		}
		else
		{
			error << "Render initialize failed. reason=backend_not_initialized_by_module" << lineBreak;
		}
		runtimeExitCode = FrameworkRuntimeExitCode::backendFlowRuntimeFailure;
		executionCompleted = true;
		return false;
	}

	CommandList* initialCommandList = renderBackend->acquireCommandList();
	const bool rendererBindingFailed =
		initialCommandList == nullptr
		|| renderBackend->getCommandQueue() == nullptr
		|| renderBackend->getSyncObject() == nullptr
		|| renderBackend->getSwapChain() == nullptr;
	if (initialCommandList != nullptr)
	{
		renderBackend->releaseCommandList(initialCommandList);
	}

	if (rendererBindingFailed)
	{
		if (backendCliFlow)
		{
			error << "[BackendCLI][Error] stage=create reason=renderer_bind_failed" << lineBreak;
		}
		else
		{
			error << "Render initialize failed. reason=renderer_bind_failed" << lineBreak;
		}
		renderBackendModule->destroyBackend();
		runtimeExitCode = FrameworkRuntimeExitCode::backendFlowRuntimeFailure;
		executionCompleted = true;
		return false;
	}

	backendTestState.finalizePending = false;
	if (backendCliFlow)
	{
		output << "[BackendCLI][Create] device=ok swapchain=ok" << lineBreak;
	}
	return true;
}

void Framework::onWindowResize(const uint32 width, const uint32 height)
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();

	if (executionFlow == FrameworkExecutionFlow::testFlow)
	{
		output << "Window resized to " << width << "x" << height << lineBreak;
		return;
	}

	if (renderBackendModule == nullptr || !renderBackendModule->isBackendCreated())
	{
		return;
	}

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	if (renderBackend == nullptr)
	{
		return;
	}

	SyncObject* syncObject = renderBackend->getSyncObject();
	if (syncObject != nullptr)
	{
		syncObject->wait();
	}

	SwapChain* swapChain = renderBackend->getSwapChain();
	if (swapChain != nullptr && swapChain->resize(width, height))
	{
		if (executionFlow == FrameworkExecutionFlow::backendFlow)
		{
			++backendTestState.resizeCount;
			backendTestState.resizeFailed = false;
			output << "[BackendCLI][Resize] width=" << width
				   << " height=" << height
				   << " status=ok" << lineBreak;
		}
		return;
	}

	if (executionFlow == FrameworkExecutionFlow::backendFlow)
	{
		backendTestState.resizeFailed = true;
		error << "[BackendCLI][Error] stage=resize reason=resize_failed" << lineBreak;
	}
	else
	{
		return;
	}
}

bool Framework::tickBackendFlow(const float deltaTimeSeconds)
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();

	if (windowsWindowObject == nullptr
		|| renderBackendModule == nullptr
		|| !renderBackendModule->isBackendCreated())
	{
		runtimeExitCode = FrameworkRuntimeExitCode::backendFlowRuntimeFailure;
		executionCompleted = true;
		return false;
	}

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	if (renderBackend == nullptr)
	{
		runtimeExitCode = FrameworkRuntimeExitCode::backendFlowRuntimeFailure;
		executionCompleted = true;
		return false;
	}

	if (windowsWindowObject->isWindowMinimized())
	{
		return true;
	}

	if (backendOptions.forceResize && !backendTestState.forcedResizeSubmitted && backendTestState.renderedFrameCount == 10)
	{
		const uint32 forcedWidth = windowsWindowObject->getClientWidth() + 32;
		const uint32 forcedHeight = windowsWindowObject->getClientHeight() + 32;
		SetWindowPos(
			windowsWindowObject->getWindowHandle(),
			nullptr,
			0,
			0,
			static_cast<int32>(forcedWidth),
			static_cast<int32>(forcedHeight),
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		backendTestState.forcedResizeSubmitted = true;
	}

	if (backendTestState.resizeFailed)
	{
		finalizeBackendFlow(false);
		return false;
	}

	unused(deltaTimeSeconds);
	if (!enqueueBackendRenderFrameCommand())
	{
		error << "[BackendCLI][Error] stage=render reason=command_list_acquire_failed" << lineBreak;
		finalizeBackendFlow(false);
		return false;
	}

	output << "[BackendCLI][Frame] index=" << backendTestState.renderedFrameCount << " present=ok" << lineBreak;
	++backendTestState.renderedFrameCount;

	if (backendTestState.renderedFrameCount >= backendOptions.frameCount)
	{
		backendTestState.finalizePending = true;
	}

	return true;
}

bool Framework::updateBackendExecutionFlow()
{
	preUpdateModules();
	const float deltaTimeSeconds = static_cast<float>(Timer::get()->getDeltaTime());
	const bool updateResult = tickBackendFlow(deltaTimeSeconds);
	postUpdateModules();
	return updateResult;
}

bool Framework::enqueueBackendRenderFrameCommand()
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	if (windowsWindowObject == nullptr
		|| renderBackendModule == nullptr
		|| !renderBackendModule->isBackendCreated())
	{
		return false;
	}

	if (windowsWindowObject->isWindowMinimized())
	{
		return true;
	}

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	if (renderBackend == nullptr)
	{
		return false;
	}

	RenderCommand& renderCommand = RenderCommand::get();
	renderCommand.enqueue("MeshUpload", [](string&& commandName, RenderBackend& renderBackendReference)
	{
		unused(commandName);
		MeshStreaming::get()->flushGpuRequests(renderBackendReference);
	});

	renderCommand.enqueue("Render", [this](string&& commandName, RenderBackend& renderBackendReference)
	{
		unused(commandName);
		renderBackendReference.executeQueuedCommandLists();
		renderBackendReference.releaseQueuedRenderResources();

		CommandList* commandList = renderBackendReference.acquireCommandList();
		if (commandList == nullptr)
		{
			backendTestState.resizeFailed = true;
			error << "[BackendCLI][Error] stage=render reason=command_list_acquire_failed" << lineBreak;
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

	return true;
}

void Framework::finalizeBackendFlow(const bool passState)
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();

	if (executionCompleted)
	{
		return;
	}

	const bool backendPass =
		passState
		&& !backendTestState.resizeFailed
		&& !backendTestState.validationFailed
		&& (backendTestState.renderedFrameCount == backendOptions.frameCount);
	output << "[BackendCLI][Summary] frameCount=" << backendTestState.renderedFrameCount
		   << " resizeCount=" << backendTestState.resizeCount
		   << " validationFailed=" << (backendTestState.validationFailed ? 1 : 0)
		   << " result=" << (backendPass ? "pass" : "fail") << lineBreak;

	if (renderBackendModule != nullptr)
	{
		renderBackendModule->destroyBackend();
	}

	backendTestState.finalizePending = false;
	executionCompleted = true;
	runtimeExitCode = backendPass
		? FrameworkRuntimeExitCode::success
		: FrameworkRuntimeExitCode::backendFlowSummaryFailure;
}
