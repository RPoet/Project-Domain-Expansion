#include "Engine/Framework/Framework.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Platform/PlatformDefine.h"
#include "Engine/Window/WindowsWindowObject.h"
#include "Render/RenderWorld.h"

struct ApplicationRunOptions
{
	RenderBackendType backendType = RenderBackendType::dx12;
#if defined(_DEBUG)
	bool enableBackendDebugLayer = true;
#else
	bool enableBackendDebugLayer = false;
#endif
	BackendValidationInjectMode backendValidationInjectMode = BackendValidationInjectMode::none;
};

enum class ApplicationExitCode : int32
{
	success = 0,
	windowCreateFailed = -1,
	frameworkInitializeFailed = -2,
	renderWorldInitializeFailed = -3,
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

	if (tryGetArgumentValue(commandLineText, L"-backend_api=", argumentValue))
	{
		RenderBackendType parsedBackendType = RenderBackendType::dx12;
		if (parseBackendType(argumentValue, parsedBackendType))
		{
			applicationRunOptions.backendType = parsedBackendType;
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

	if (tryGetArgumentValue(commandLineText, L"-backend_validation_inject=", argumentValue))
	{
		BackendValidationInjectMode parsedInjectMode = BackendValidationInjectMode::none;
		const bool validInjectMode = parseBackendValidationInjectMode(argumentValue, parsedInjectMode);
		assert(validInjectMode && "[Main][Assert] reason=backend_validation_inject_mode_invalid");
		applicationRunOptions.backendValidationInjectMode = parsedInjectMode;
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
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	uint32 runtimeWindowWidth = 0;
	uint32 runtimeWindowHeight = 0;
	if (diskLoaderModule->TEMP_loadRuntimeWindowResolution(runtimeWindowWidth, runtimeWindowHeight))
	{
		windowCreateOptions.initialClientWidth = static_cast<int32>(runtimeWindowWidth);
		windowCreateOptions.initialClientHeight = static_cast<int32>(runtimeWindowHeight);
	}

	WindowsWindowObject windowsWindowObject;
	const bool createdMainWindow = windowsWindowObject.create(windowCreateOptions);
	assert(createdMainWindow && "[Main][Assert] reason=main_window_create_failed");
	diskLoaderModule->TEMP_saveRuntimeWindowResolution(
		windowsWindowObject.getClientWidth(),
		windowsWindowObject.getClientHeight());

	FrameworkInitializeOptions frameworkInitializeOptions = {};
	frameworkInitializeOptions.backendOptions.backendType = applicationRunOptions.backendType;
	frameworkInitializeOptions.backendOptions.enableDebugLayer = applicationRunOptions.enableBackendDebugLayer;
	frameworkInitializeOptions.backendOptions.validationInjectMode = applicationRunOptions.backendValidationInjectMode;

	Framework framework = {};
	framework.registerModule(frameworkInitializeOptions);
	if (!framework.initialize(windowsWindowObject, frameworkInitializeOptions))
	{
		framework.shutdown();
		windowsWindowObject.destroy();
		return resolveFrameworkInitializeExitCode(
			framework.getRuntimeExitCode(),
			ApplicationExitCode::frameworkInitializeFailed);
	}

	RenderWorld renderWorld = {};
	if (!renderWorld.initialize(windowsWindowObject))
	{
		renderWorld.shutdown();
		framework.shutdown();
		windowsWindowObject.destroy();
		return resolveFrameworkInitializeExitCode(
			framework.getRuntimeExitCode(),
			ApplicationExitCode::renderWorldInitializeFailed);
	}

	while (windowsWindowObject.pumpMessages())
	{
		const bool updatedFramework = framework.update();
		assert(updatedFramework && "[Main][Assert] reason=framework_update_failed");

		RenderWorldUpdateInput renderWorldUpdateInput = {};
		renderWorldUpdateInput.worldFlow = true;
		renderWorldUpdateInput.worldUpdateSerial = framework.getWorldUpdateSerial();
		renderWorldUpdateInput.renderCommandFlushInput.clearOnly = false;

		const bool updatedRenderWorld = renderWorld.update(renderWorldUpdateInput);
		assert(updatedRenderWorld && "[Main][Assert] reason=render_world_update_failed");

		Sleep(1);
	}

	renderWorld.shutdown();
	framework.shutdown();
	diskLoaderModule->TEMP_saveRuntimeWindowResolution(
		windowsWindowObject.getClientWidth(),
		windowsWindowObject.getClientHeight());
	windowsWindowObject.destroy();
	return static_cast<int32>(framework.getRuntimeExitCode());
}
