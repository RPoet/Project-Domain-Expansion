#include "Engine/Framework/Framework.h"
#include "Engine/Framework/BackendValidation.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Engine/Window/WindowsWindowObject.h"
#include "EngineTests/Framework/FrameworkBackendTest.h"
#include "EngineTests/Framework/FrameworkTest.h"
#include "EngineTests/Framework/TestFramework.h"
#include "EngineTests/FrameworkTestRegistration.h"
#include "Render/RenderCommand.h"
#include "Render/Renderer.h"
#include "Render/RenderWorld.h"
#include "Render/Screen.h"

enum class EngineTestsRunMode : uint32
{
	frameworkMode = 0,
	backendMode = 1,
};

struct EngineTestsRunOptions
{
	EngineTestsRunMode runMode = EngineTestsRunMode::frameworkMode;
	RenderBackendType backendType = RenderBackendType::dx12;
	uint32 backendFrameCount = 120;
	bool forceResize = false;
#if defined(_DEBUG)
	bool enableBackendDebugLayer = true;
#else
	bool enableBackendDebugLayer = false;
#endif
	BackendValidationInjectMode backendValidationInjectMode = BackendValidationInjectMode::none;
};

enum class EngineTestsExecutionCode : int32
{
	success = 0,
	testTickFailed = 1,
	testFailed = 2,
	frameworkInitializeFailed = 3,
	renderWorldInitializeFailed = 4,
	renderWorldUpdateFailed = 5,
};

enum class EngineTestsBackendExecutionCode : int32
{
	success = 0,
	runtimeFailure = 3,
	summaryFailure = 4,
	debugValidationFailure = 5,
};

struct EngineTestsBackendState
{
	uint32 renderedFrameCount = 0;
	uint32 resizeCount = 0;
	bool resizeFailed = false;
	bool validationFailed = false;
	bool forcedResizeSubmitted = false;
	bool finalizePending = false;

	void reset()
	{
		renderedFrameCount = 0;
		resizeCount = 0;
		resizeFailed = false;
		validationFailed = false;
		forcedResizeSubmitted = false;
		finalizePending = false;
	}
};

static const char* getEngineTestsBackendTypeText(const RenderBackendType backendType)
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

static string normalizeEngineTestsValidationText(const string& text)
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

static bool processEngineTestsBackendValidationFailFast(
	const Framework& framework,
	const char* flowText)
{
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	const bool validModule = renderBackendModule != nullptr
		&& renderBackendModule->isBackendCreated()
		&& renderBackendModule->getBackend() != nullptr;
	assert(validModule && "[EngineTests][Assert] reason=backend_validation_module_missing");

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	const FrameworkBackendOptions& backendOptions = framework.getBackendOptions();
	const BackendValidationPollResult pollResult = collectBackendMessages(
		backendOptions.backendType,
		renderBackend,
		backendOptions.validationInjectMode);

	const char* selectedFlowText = flowText != nullptr && flowText[0] != '\0'
		? flowText
		: "unknown";
	const char* backendText = getEngineTestsBackendTypeText(backendOptions.backendType);
	for (uint32 messageIndex = 0; messageIndex < static_cast<uint32>(pollResult.messages.size()); ++messageIndex)
	{
		const BackendValidationMessage& message = pollResult.messages[messageIndex];
		const bool failingMessage = isBackendValidationFailingSeverity(message.severity);
		const string messageId = message.id.empty() ? "unknown" : message.id;
		output_stream& selectedStream = failingMessage ? error : output;
		selectedStream << "[BackendValidation][Message] flow=" << selectedFlowText
					   << " backend=" << backendText
					   << " severity=" << getBackendValidationSeverityText(message.severity)
					   << " id=" << messageId
					   << " action=" << (failingMessage ? "fail" : "log")
					   << " text=" << normalizeEngineTestsValidationText(message.text) << lineBreak;
	}

	const bool validationFailed = pollResult.failingCount > 0;
	output_stream& summaryStream = validationFailed ? error : output;
	summaryStream << "[BackendValidation][Summary] flow=" << selectedFlowText
				  << " backend=" << backendText
				  << " total=" << pollResult.totalCount
				  << " failing=" << pollResult.failingCount
				  << " threshold=warning"
				  << " result=" << (validationFailed ? "fail" : "pass") << lineBreak;
	return validationFailed;
}

static bool isEngineTestsWhitespaceCharacter(const wide_character character)
{
	return character == L' ' || character == L'\t';
}

static bool tryGetEngineTestsArgumentValue(
	const wstring& commandLineText,
	const wstring& argumentKey,
	wstring& argumentValue)
{
	size_t keyPosition = commandLineText.find(argumentKey);
	while (keyPosition != wstring::npos)
	{
		if (keyPosition > 0 && !isEngineTestsWhitespaceCharacter(commandLineText[keyPosition - 1]))
		{
			keyPosition = commandLineText.find(argumentKey, keyPosition + 1);
			continue;
		}

		const size_t valueStart = keyPosition + argumentKey.length();
		size_t valueEnd = commandLineText.find_first_of(L" \t", valueStart);
		if (valueEnd == wstring::npos)
		{
			valueEnd = commandLineText.length();
		}

		if (valueEnd <= valueStart)
		{
			return false;
		}

		argumentValue = commandLineText.substr(valueStart, valueEnd - valueStart);
		return true;
	}

	return false;
}

static bool parseEngineTestsUnsignedInteger(const wstring& textValue, uint32& parsedValue)
{
	if (textValue.empty())
	{
		return false;
	}

	unsigned long long value = 0;
	for (size_t characterIndex = 0; characterIndex < textValue.length(); ++characterIndex)
	{
		const wide_character character = textValue[characterIndex];
		if (character < L'0' || character > L'9')
		{
			return false;
		}

		value = (value * 10ull) + static_cast<unsigned long long>(character - L'0');
		if (value > static_cast<unsigned long long>(0xFFFFFFFFu))
		{
			return false;
		}
	}

	parsedValue = static_cast<uint32>(value);
	return true;
}

static bool parseEngineTestsBackendType(const wstring& backendTypeText, RenderBackendType& backendType)
{
	if (backendTypeText == L"dx12")
	{
		backendType = RenderBackendType::dx12;
		return true;
	}

	if (backendTypeText == L"vulkan")
	{
		backendType = RenderBackendType::vulkan;
		return true;
	}

	if (backendTypeText == L"metal")
	{
		backendType = RenderBackendType::metal;
		return true;
	}

	return false;
}

static bool parseEngineTestsBackendValidationInjectMode(
	const wstring& injectModeText,
	BackendValidationInjectMode& injectMode)
{
	if (injectModeText == L"none")
	{
		injectMode = BackendValidationInjectMode::none;
		return true;
	}

	if (injectModeText == L"warning")
	{
		injectMode = BackendValidationInjectMode::warning;
		return true;
	}

	if (injectModeText == L"error")
	{
		injectMode = BackendValidationInjectMode::error;
		return true;
	}

	return false;
}

static EngineTestsRunOptions parseEngineTestsRunOptions(const WideStringPointer commandLine)
{
	EngineTestsRunOptions runOptions = {};

	const wstring commandLineText = commandLine != nullptr ? commandLine : L"";
	wstring argumentValue = {};
	if (tryGetEngineTestsArgumentValue(commandLineText, L"-mode=", argumentValue))
	{
		if (argumentValue == L"backend")
		{
			runOptions.runMode = EngineTestsRunMode::backendMode;
		}
	}

	if (tryGetEngineTestsArgumentValue(commandLineText, L"-backend_api=", argumentValue))
	{
		RenderBackendType parsedBackendType = RenderBackendType::dx12;
		if (parseEngineTestsBackendType(argumentValue, parsedBackendType))
		{
			runOptions.backendType = parsedBackendType;
		}
	}

	if (tryGetEngineTestsArgumentValue(commandLineText, L"-frames=", argumentValue))
	{
		uint32 parsedFrameCount = 0;
		if (parseEngineTestsUnsignedInteger(argumentValue, parsedFrameCount) && parsedFrameCount > 0)
		{
			runOptions.backendFrameCount = parsedFrameCount;
		}
	}

	if (tryGetEngineTestsArgumentValue(commandLineText, L"-backend_debug=", argumentValue))
	{
		uint32 parsedDebugFlag = 0;
		if (parseEngineTestsUnsignedInteger(argumentValue, parsedDebugFlag))
		{
			runOptions.enableBackendDebugLayer = parsedDebugFlag != 0;
		}
	}

	if (tryGetEngineTestsArgumentValue(commandLineText, L"-force_resize=", argumentValue))
	{
		uint32 parsedForceResizeFlag = 0;
		if (parseEngineTestsUnsignedInteger(argumentValue, parsedForceResizeFlag))
		{
			runOptions.forceResize = parsedForceResizeFlag != 0;
		}
	}

	if (tryGetEngineTestsArgumentValue(commandLineText, L"-backend_validation_inject=", argumentValue))
	{
		BackendValidationInjectMode parsedInjectMode = BackendValidationInjectMode::none;
		if (parseEngineTestsBackendValidationInjectMode(argumentValue, parsedInjectMode))
		{
			runOptions.backendValidationInjectMode = parsedInjectMode;
		}
	}

	return runOptions;
}

static bool createAndLoadEngineTestsWorld(
	Framework& framework,
	const wstring& worldName,
	uint32& outWorldIndex)
{
	outWorldIndex = framework.createWorld(worldName);
	return outWorldIndex != invalidWorldIndex && framework.loadWorld(outWorldIndex);
}

static bool ensureEngineTestsActiveWorld(
	Framework& framework,
	uint32& fallbackWorldIndex,
	const wstring& worldName)
{
	if (framework.getActiveWorld() != nullptr)
	{
		return true;
	}

	const bool fallbackWorldReady =
		fallbackWorldIndex != invalidWorldIndex
		&& framework.getWorld(fallbackWorldIndex) != nullptr;
	if (!fallbackWorldReady)
	{
		return createAndLoadEngineTestsWorld(framework, worldName, fallbackWorldIndex);
	}

	return framework.loadWorld(fallbackWorldIndex);
}

class EngineTestsBackendRunner final
{
public:
	EngineTestsBackendRunner(
		Framework& framework,
		WindowsWindowObject& windowsWindowObject,
		const EngineTestsRunOptions& runOptions)
		: framework(framework)
		, windowsWindowObject(windowsWindowObject)
		, backendType(runOptions.backendType)
		, targetFrameCount(runOptions.backendFrameCount)
		, forceResize(runOptions.forceResize)
	{
	}

	bool initialize()
	{
		backendState.reset();
		executionCompleted = false;
		executionCode = EngineTestsBackendExecutionCode::success;
		observedSwapChainWidth = 0;
		observedSwapChainHeight = 0;
		resizeMismatchFrameCount = 0;

		output << "[BackendCLI][Begin] mode=backend api="
			   << getEngineTestsBackendTypeText(backendType)
			   << " frames=" << targetFrameCount << lineBreak;

		shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
		const bool validModule = renderBackendModule != nullptr
			&& renderBackendModule->isBackendCreated()
			&& renderBackendModule->getBackend() != nullptr;
		if (!validModule)
		{
			error << "[BackendCLI][Error] stage=create reason=backend_module_missing" << lineBreak;
			complete(EngineTestsBackendExecutionCode::runtimeFailure, false);
			return false;
		}

		RenderBackend* renderBackend = renderBackendModule->getBackend();
		CommandList* initialCommandList = renderBackend->acquireCommandList();
		const bool rendererBindingReady =
			initialCommandList != nullptr
			&& renderBackend->getCommandQueue() != nullptr
			&& renderBackend->getSyncObject() != nullptr
			&& renderBackend->getSwapChain() != nullptr;
		if (initialCommandList != nullptr)
		{
			renderBackend->releaseCommandList(initialCommandList);
		}

		if (!rendererBindingReady)
		{
			error << "[BackendCLI][Error] stage=create reason=renderer_bind_failed" << lineBreak;
			complete(EngineTestsBackendExecutionCode::runtimeFailure, false);
			return false;
		}

		SwapChain* swapChain = renderBackend->getSwapChain();
		if (swapChain != nullptr)
		{
			observedSwapChainWidth = swapChain->getWidth();
			observedSwapChainHeight = swapChain->getHeight();
		}

		output << "[BackendCLI][Create] device=ok swapchain=ok" << lineBreak;
		return true;
	}

	bool tick()
	{
		if (executionCompleted)
		{
			return true;
		}

		shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
		const bool validBackend = renderBackendModule != nullptr
			&& renderBackendModule->isBackendCreated()
			&& renderBackendModule->getBackend() != nullptr;
		if (!validBackend)
		{
			error << "[BackendCLI][Error] stage=runtime reason=backend_missing" << lineBreak;
			complete(EngineTestsBackendExecutionCode::runtimeFailure, false);
			return false;
		}

		if (windowsWindowObject.isWindowMinimized())
		{
			return true;
		}

		submitForcedResizeIfNeeded();
		updateResizeState();
		if (backendState.resizeFailed)
		{
			complete(EngineTestsBackendExecutionCode::summaryFailure, false);
			return false;
		}

		if (!enqueueRenderFrameCommand())
		{
			error << "[BackendCLI][Error] stage=render reason=command_list_acquire_failed" << lineBreak;
			complete(EngineTestsBackendExecutionCode::summaryFailure, false);
			return false;
		}

		output << "[BackendCLI][Frame] index=" << backendState.renderedFrameCount << " present=ok" << lineBreak;
		++backendState.renderedFrameCount;
		if (backendState.renderedFrameCount >= targetFrameCount)
		{
			backendState.finalizePending = true;
		}

		return true;
	}

	void handleValidationFailure()
	{
		backendState.validationFailed = true;
		complete(EngineTestsBackendExecutionCode::debugValidationFailure, false);
	}

	void onRenderCommandsFlushed()
	{
		if (!backendState.finalizePending)
		{
			return;
		}

		backendState.finalizePending = false;
		complete(EngineTestsBackendExecutionCode::success, true);
	}

	bool isExecutionCompleted() const
	{
		return executionCompleted;
	}

	EngineTestsBackendExecutionCode getExecutionCode() const
	{
		assert(executionCompleted && "[EngineTests][Assert] reason=backend_execution_code_requested_before_completion");
		return executionCode;
	}

	void complete(const EngineTestsBackendExecutionCode exitCode, const bool passState)
	{
		if (executionCompleted)
		{
			return;
		}

		const bool backendPass =
			passState
			&& !backendState.resizeFailed
			&& !backendState.validationFailed
			&& backendState.renderedFrameCount == targetFrameCount;
		output << "[BackendCLI][Summary] frameCount=" << backendState.renderedFrameCount
			   << " resizeCount=" << backendState.resizeCount
			   << " validationFailed=" << (backendState.validationFailed ? 1 : 0)
			   << " result=" << (backendPass ? "pass" : "fail") << lineBreak;

		shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
		if (renderBackendModule != nullptr)
		{
			renderBackendModule->destroyBackend();
		}

		backendState.finalizePending = false;
		executionCode = backendPass
			? EngineTestsBackendExecutionCode::success
			: exitCode;
		executionCompleted = true;
	}

private:
	void submitForcedResizeIfNeeded()
	{
		if (!forceResize
			|| backendState.forcedResizeSubmitted
			|| backendState.renderedFrameCount != 10)
		{
			return;
		}

		const uint32 forcedWidth = windowsWindowObject.getClientWidth() + 32;
		const uint32 forcedHeight = windowsWindowObject.getClientHeight() + 32;
		SetWindowPos(
			windowsWindowObject.getWindowHandle(),
			nullptr,
			0,
			0,
			static_cast<int32>(forcedWidth),
			static_cast<int32>(forcedHeight),
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		backendState.forcedResizeSubmitted = true;
	}

	void updateResizeState()
	{
		if (!backendState.forcedResizeSubmitted)
		{
			return;
		}

		shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
		const bool validBackend = renderBackendModule != nullptr
			&& renderBackendModule->isBackendCreated()
			&& renderBackendModule->getBackend() != nullptr;
		if (!validBackend)
		{
			return;
		}

		SwapChain* swapChain = renderBackendModule->getBackend()->getSwapChain();
		if (swapChain == nullptr)
		{
			return;
		}

		const uint32 currentWindowWidth = windowsWindowObject.getClientWidth();
		const uint32 currentWindowHeight = windowsWindowObject.getClientHeight();
		const bool windowSizeChanged =
			currentWindowWidth != observedSwapChainWidth
			|| currentWindowHeight != observedSwapChainHeight;
		if (!windowSizeChanged)
		{
			return;
		}

		const bool resizeApplied =
			swapChain->getWidth() == currentWindowWidth
			&& swapChain->getHeight() == currentWindowHeight;
		if (resizeApplied)
		{
			observedSwapChainWidth = swapChain->getWidth();
			observedSwapChainHeight = swapChain->getHeight();
			resizeMismatchFrameCount = 0;
			backendState.resizeFailed = false;
			++backendState.resizeCount;
			output << "[BackendCLI][Resize] width=" << observedSwapChainWidth
				   << " height=" << observedSwapChainHeight
				   << " status=ok" << lineBreak;
			return;
		}

		++resizeMismatchFrameCount;
		if (resizeMismatchFrameCount < 2)
		{
			return;
		}

		backendState.resizeFailed = true;
		error << "[BackendCLI][Error] stage=resize reason=resize_failed" << lineBreak;
	}

	bool enqueueRenderFrameCommand()
	{
		shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
		const bool validBackend = renderBackendModule != nullptr
			&& renderBackendModule->isBackendCreated()
			&& renderBackendModule->getBackend() != nullptr;
		if (!validBackend)
		{
			return false;
		}

		if (windowsWindowObject.isWindowMinimized())
		{
			return true;
		}

		RenderCommand& renderCommand = RenderCommand::get();
		renderCommand.enqueue("Render", [this](string&& commandName, RenderBackend& renderBackendReference)
		{
			unused(commandName);
			renderBackendReference.executeQueuedCommandLists();
			renderBackendReference.releaseQueuedRenderResources();

			CommandList* commandList = renderBackendReference.acquireCommandList();
			if (commandList == nullptr)
			{
				backendState.resizeFailed = true;
				error << "[BackendCLI][Error] stage=render reason=command_list_acquire_failed" << lineBreak;
				return;
			}

			Renderer renderer = {};
			Screen screen = {};
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

	Framework& framework;
	WindowsWindowObject& windowsWindowObject;
	RenderBackendType backendType = RenderBackendType::dx12;
	uint32 targetFrameCount = 0;
	bool forceResize = false;
	EngineTestsBackendState backendState = {};
	bool executionCompleted = false;
	EngineTestsBackendExecutionCode executionCode = EngineTestsBackendExecutionCode::success;
	uint32 observedSwapChainWidth = 0;
	uint32 observedSwapChainHeight = 0;
	uint32 resizeMismatchFrameCount = 0;
};

static int32 runFrameworkTests(WindowsWindowObject& windowsWindowObject)
{
	FrameworkTest::InitializeOptions frameworkInitializeOptions = {};
	FrameworkTest framework = {};
	if (!framework.initialize(windowsWindowObject, frameworkInitializeOptions))
	{
		const FrameworkRuntimeExitCode initializeExitCode = framework.getRuntimeExitCode();
		framework.shutdown();
		return resolveFrameworkInitializeExitCode(
			initializeExitCode,
			EngineTestsExecutionCode::frameworkInitializeFailed);
	}

	uint32 fallbackWorldIndex = invalidWorldIndex;
	if (!createAndLoadEngineTestsWorld(framework, L"FrameworkTestRuntime", fallbackWorldIndex))
	{
		framework.shutdown();
		return static_cast<int32>(EngineTestsExecutionCode::frameworkInitializeFailed);
	}

	RenderWorld renderWorld = {};
	if (!renderWorld.initialize(windowsWindowObject))
	{
		renderWorld.shutdown();
		framework.shutdown();
		return static_cast<int32>(EngineTestsExecutionCode::renderWorldInitializeFailed);
	}

	TestFramework testFramework = {};
	registerFrameworkTests(testFramework);

	EngineTestsExecutionCode executionCode = EngineTestsExecutionCode::success;
	while (windowsWindowObject.pumpMessages())
	{
		if (!ensureEngineTestsActiveWorld(framework, fallbackWorldIndex, L"FrameworkTestRuntime"))
		{
			error << "Framework test active world restore failed." << lineBreak;
			executionCode = EngineTestsExecutionCode::testTickFailed;
			break;
		}

		if (!framework.update())
		{
			error << "Framework update failed during tests." << lineBreak;
			executionCode = EngineTestsExecutionCode::testTickFailed;
			break;
		}

		if (!testFramework.tick(framework))
		{
			error << "Test framework tick failed." << lineBreak;
			executionCode = EngineTestsExecutionCode::testTickFailed;
			break;
		}

		RenderWorldUpdateInput renderWorldUpdateInput = {};
		renderWorldUpdateInput.worldFlow = false;
		renderWorldUpdateInput.worldUpdateSerial = framework.getWorldUpdateSerial();
		renderWorldUpdateInput.renderCommandFlushInput.clearOnly = true;
		renderWorldUpdateInput.renderCommandFlushInput.validateAfterFlush = false;
		if (!renderWorld.update(renderWorldUpdateInput))
		{
			error << "RenderWorld update failed during tests." << lineBreak;
			executionCode = EngineTestsExecutionCode::renderWorldUpdateFailed;
			break;
		}

		if (testFramework.isCompleted())
		{
			break;
		}

		Sleep(1);
	}

	const FrameworkTestSummary& frameworkTestSummary = testFramework.getSummary();
	if (executionCode == EngineTestsExecutionCode::success
		&& frameworkTestSummary.failedTestCaseCount > 0)
	{
		executionCode = EngineTestsExecutionCode::testFailed;
	}

	if (frameworkTestSummary.failedTestCaseCount == 0)
	{
		output << "Framework tests completed successfully. passedCase="
			   << frameworkTestSummary.passedTestCaseCount
			   << ", totalCase=" << frameworkTestSummary.totalTestCaseCount << lineBreak;
	}
	else
	{
		error << "Framework tests failed. failedCase=" << frameworkTestSummary.failedTestCaseCount
			  << ", failedAssertion=" << frameworkTestSummary.failedAssertionCount
			  << ", totalAssertion=" << frameworkTestSummary.totalAssertionCount << lineBreak;
	}

	renderWorld.shutdown();
	framework.shutdown();
	return static_cast<int32>(executionCode);
}

static int32 runBackendTests(
	WindowsWindowObject& windowsWindowObject,
	const EngineTestsRunOptions& runOptions)
{
	FrameworkBackendTest::InitializeOptions frameworkInitializeOptions = {};
	frameworkInitializeOptions.backendType = runOptions.backendType;
	frameworkInitializeOptions.enableDebugLayer = runOptions.enableBackendDebugLayer;
	frameworkInitializeOptions.validationInjectMode = runOptions.backendValidationInjectMode;

	FrameworkBackendTest framework = {};
	if (!framework.initialize(windowsWindowObject, frameworkInitializeOptions))
	{
		const FrameworkRuntimeExitCode initializeExitCode = framework.getRuntimeExitCode();
		framework.shutdown();
		return resolveFrameworkInitializeExitCode(
			initializeExitCode,
			EngineTestsExecutionCode::frameworkInitializeFailed);
	}

	uint32 fallbackWorldIndex = invalidWorldIndex;
	if (!createAndLoadEngineTestsWorld(framework, L"BackendTestRuntime", fallbackWorldIndex))
	{
		framework.shutdown();
		return static_cast<int32>(EngineTestsBackendExecutionCode::runtimeFailure);
	}

	RenderWorld renderWorld = {};
	if (!renderWorld.initialize(windowsWindowObject))
	{
		renderWorld.shutdown();
		framework.shutdown();
		return static_cast<int32>(EngineTestsExecutionCode::renderWorldInitializeFailed);
	}

	EngineTestsBackendRunner backendRunner(framework, windowsWindowObject, runOptions);
	if (!backendRunner.initialize())
	{
		const EngineTestsBackendExecutionCode executionCode = backendRunner.getExecutionCode();
		renderWorld.shutdown();
		framework.shutdown();
		return static_cast<int32>(executionCode);
	}

	while (windowsWindowObject.pumpMessages())
	{
		if (!ensureEngineTestsActiveWorld(framework, fallbackWorldIndex, L"BackendTestRuntime"))
		{
			error << "[BackendCLI][Error] stage=update reason=active_world_restore_failed" << lineBreak;
			backendRunner.complete(EngineTestsBackendExecutionCode::runtimeFailure, false);
			break;
		}

		if (!framework.update())
		{
			error << "[BackendCLI][Error] stage=update reason=framework_update_failed" << lineBreak;
			backendRunner.complete(EngineTestsBackendExecutionCode::runtimeFailure, false);
			break;
		}

		if (!backendRunner.tick())
		{
			break;
		}

		RenderWorldUpdateInput renderWorldUpdateInput = {};
		renderWorldUpdateInput.worldFlow = false;
		renderWorldUpdateInput.worldUpdateSerial = framework.getWorldUpdateSerial();
		renderWorldUpdateInput.renderCommandFlushInput.clearOnly = false;
		renderWorldUpdateInput.renderCommandFlushInput.validateAfterFlush = true;
		renderWorldUpdateInput.renderCommandFlushInput.processBackendValidationFailFast = [&framework, &backendRunner]() -> bool
		{
			const bool validationFailed = processEngineTestsBackendValidationFailFast(framework, "backend");
			if (validationFailed)
			{
				backendRunner.handleValidationFailure();
			}

			return validationFailed;
		};
		renderWorldUpdateInput.renderCommandFlushInput.onFlushed = [&backendRunner]()
		{
			backendRunner.onRenderCommandsFlushed();
		};

		if (!renderWorld.update(renderWorldUpdateInput))
		{
			error << "[BackendCLI][Error] stage=render reason=render_world_update_failed" << lineBreak;
			backendRunner.complete(EngineTestsBackendExecutionCode::runtimeFailure, false);
			break;
		}

		if (backendRunner.isExecutionCompleted())
		{
			break;
		}

		Sleep(1);
	}

	if (!backendRunner.isExecutionCompleted())
	{
		backendRunner.complete(EngineTestsBackendExecutionCode::summaryFailure, false);
	}

	const EngineTestsBackendExecutionCode executionCode = backendRunner.getExecutionCode();
	renderWorld.shutdown();
	framework.shutdown();
	return static_cast<int32>(executionCode);
}

int WINAPI wWinMain(
	HandleInstance windowInstanceHandle,
	HandleInstance previousWindowInstanceHandle,
	WideStringPointer commandLine,
	int commandShow)
{
	unused(windowInstanceHandle);
	unused(previousWindowInstanceHandle);
	unused(commandShow);

	const EngineTestsRunOptions runOptions = parseEngineTestsRunOptions(commandLine);

	WindowCreateOptions windowCreateOptions = {};
	windowCreateOptions.windowTitle = L"DomainExpansion Engine Tests";
	windowCreateOptions.initialClientWidth = 1600;
	windowCreateOptions.initialClientHeight = 900;
	windowCreateOptions.startVisible = true;
	windowCreateOptions.startBorderlessFullscreen = false;

	WindowsWindowObject windowsWindowObject = {};
	if (!windowsWindowObject.create(windowCreateOptions))
	{
		error << "Failed to create test window." << lineBreak;
		return static_cast<int32>(EngineTestsExecutionCode::frameworkInitializeFailed);
	}

	const int32 executionCode =
		runOptions.runMode == EngineTestsRunMode::backendMode
		? runBackendTests(windowsWindowObject, runOptions)
		: runFrameworkTests(windowsWindowObject);
	windowsWindowObject.destroy();
	return executionCode;
}
