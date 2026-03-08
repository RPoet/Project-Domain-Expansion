#include "Engine/Framework/Framework.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Engine/Window/WindowsWindowObject.h"
#include "Render/RenderWorld.h"

enum class ApplicationRunMode : uint32
{
	worldMode = 0,
	testMode = 1,
	backendMode = 2,
};

struct ApplicationRunOptions
{
	ApplicationRunMode runMode = ApplicationRunMode::worldMode;
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

static bool isWhitespaceCharacter(const wide_character character)
{
	return character == L' ' || character == L'\t';
}

static bool tryGetArgumentValue(
	const wstring& commandLineText,
	const wstring& argumentKey,
	wstring& argumentValue)
{
	size_t keyPosition = commandLineText.find(argumentKey);
	while (keyPosition != wstring::npos)
	{
		if (keyPosition > 0 && !isWhitespaceCharacter(commandLineText[keyPosition - 1]))
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

static bool parseUnsignedInteger(const wstring& textValue, uint32& parsedValue)
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

static bool parseBackendType(const wstring& backendTypeText, RenderBackendType& backendType)
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

static bool parseBackendValidationInjectMode(
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

static ApplicationRunOptions parseApplicationRunOptions(const WideStringPointer commandLine)
{
	ApplicationRunOptions applicationRunOptions = {};

	const wstring commandLineText = commandLine != nullptr ? commandLine : L"";
	wstring argumentValue;

	if (tryGetArgumentValue(commandLineText, L"-mode=", argumentValue))
	{
		if (argumentValue == L"backend")
		{
			applicationRunOptions.runMode = ApplicationRunMode::backendMode;
		}
		else if (argumentValue == L"test")
		{
			applicationRunOptions.runMode = ApplicationRunMode::testMode;
		}
		else if (argumentValue == L"world")
		{
			applicationRunOptions.runMode = ApplicationRunMode::worldMode;
		}
		else
		{
			error << "Unknown mode argument. Fallback to world mode." << lineBreak;
		}
	}

	if (tryGetArgumentValue(commandLineText, L"-backend_api=", argumentValue))
	{
		RenderBackendType parsedBackendType = RenderBackendType::dx12;
		if (parseBackendType(argumentValue, parsedBackendType))
		{
			applicationRunOptions.backendType = parsedBackendType;
		}
	}

	if (tryGetArgumentValue(commandLineText, L"-frames=", argumentValue))
	{
		uint32 parsedFrameCount = 0;
		if (parseUnsignedInteger(argumentValue, parsedFrameCount) && parsedFrameCount > 0)
		{
			applicationRunOptions.backendFrameCount = parsedFrameCount;
		}
	}

	if (tryGetArgumentValue(commandLineText, L"-backend_debug=", argumentValue))
	{
		uint32 parsedDebugFlag = 0;
		if (parseUnsignedInteger(argumentValue, parsedDebugFlag))
		{
			applicationRunOptions.enableBackendDebugLayer = parsedDebugFlag != 0;
		}
	}

	if (tryGetArgumentValue(commandLineText, L"-force_resize=", argumentValue))
	{
		uint32 parsedForceResizeFlag = 0;
		if (parseUnsignedInteger(argumentValue, parsedForceResizeFlag))
		{
			applicationRunOptions.forceResize = parsedForceResizeFlag != 0;
		}
	}

	if (tryGetArgumentValue(commandLineText, L"-backend_validation_inject=", argumentValue))
	{
		BackendValidationInjectMode parsedInjectMode = BackendValidationInjectMode::none;
		if (parseBackendValidationInjectMode(argumentValue, parsedInjectMode))
		{
			applicationRunOptions.backendValidationInjectMode = parsedInjectMode;
		}
		else
		{
			error << "Unknown backend validation inject mode. Fallback to none." << lineBreak;
		}
	}

	return applicationRunOptions;
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

	const ApplicationRunOptions applicationRunOptions = parseApplicationRunOptions(commandLine);

	WindowCreateOptions windowCreateOptions = {};
	windowCreateOptions.windowTitle = L"DomainExpansion Engine";
	windowCreateOptions.initialClientWidth = 1600;
	windowCreateOptions.initialClientHeight = 900;
	windowCreateOptions.startVisible = true;
	windowCreateOptions.startBorderlessFullscreen = false;

	WindowsWindowObject windowsWindowObject;
	if (!windowsWindowObject.create(windowCreateOptions))
	{
		error << "Failed to create main window." << lineBreak;
		return -1;
	}

	FrameworkInitializeOptions frameworkInitializeOptions = {};
	if (applicationRunOptions.runMode == ApplicationRunMode::backendMode)
	{
		frameworkInitializeOptions.executionFlow = FrameworkExecutionFlow::backendFlow;
	}
	else if (applicationRunOptions.runMode == ApplicationRunMode::testMode)
	{
		frameworkInitializeOptions.executionFlow = FrameworkExecutionFlow::testFlow;
	}
	else
	{
		frameworkInitializeOptions.executionFlow = FrameworkExecutionFlow::worldFlow;
	}
	frameworkInitializeOptions.backendOptions.backendType = applicationRunOptions.backendType;
	frameworkInitializeOptions.backendOptions.frameCount = applicationRunOptions.backendFrameCount;
	frameworkInitializeOptions.backendOptions.forceResize = applicationRunOptions.forceResize;
	frameworkInitializeOptions.backendOptions.enableDebugLayer = applicationRunOptions.enableBackendDebugLayer;
	frameworkInitializeOptions.backendOptions.validationInjectMode = applicationRunOptions.backendValidationInjectMode;

	Framework framework(frameworkInitializeOptions.executionFlow);
	if (!framework.initialize(windowsWindowObject, frameworkInitializeOptions))
	{
		const int32 initializeExitCode = framework.getRuntimeExitCode();
		framework.shutdown();
		windowsWindowObject.destroy();
		return initializeExitCode != 0 ? initializeExitCode : -2;
	}

	RenderWorld renderWorld = {};
	if (!renderWorld.initialize(windowsWindowObject))
	{
		const int32 initializeExitCode = framework.getRuntimeExitCode();
		renderWorld.shutdown();
		framework.shutdown();
		windowsWindowObject.destroy();
		return initializeExitCode != 0 ? initializeExitCode : -3;
	}

	while (windowsWindowObject.pumpMessages())
	{
		if (!framework.update())
		{
			error << "Framework update failed." << lineBreak;
			break;
		}

		RenderWorld::UpdateInput renderWorldUpdateInput = {};
		renderWorldUpdateInput.worldFlow = framework.getExecutionFlow() == FrameworkExecutionFlow::worldFlow;
		renderWorldUpdateInput.worldUpdateSerial = framework.getWorldUpdateSerial();
		renderWorldUpdateInput.renderCommandFlushInput.clearOnly = framework.getExecutionFlow() == FrameworkExecutionFlow::testFlow;
		renderWorldUpdateInput.renderCommandFlushInput.validateAfterFlush =
			framework.getExecutionFlow() == FrameworkExecutionFlow::worldFlow
			|| framework.getExecutionFlow() == FrameworkExecutionFlow::backendFlow;
		renderWorldUpdateInput.renderCommandFlushInput.processBackendValidationFailFast = [&framework]() -> bool
		{
			return framework.processBackendValidationFailFast();
		};
		renderWorldUpdateInput.renderCommandFlushInput.onFlushed = [&framework]()
		{
			framework.notifyRenderCommandQueueFlushed();
		};

		if (!renderWorld.update(renderWorldUpdateInput))
		{
			error << "RenderWorld update failed." << lineBreak;
			break;
		}

		if (framework.isExecutionCompleted())
		{
			break;
		}

		Sleep(1);
	}

	const int32 runtimeExitCode = framework.getRuntimeExitCode();
	renderWorld.shutdown();
	framework.shutdown();
	windowsWindowObject.destroy();
	return runtimeExitCode;
}
