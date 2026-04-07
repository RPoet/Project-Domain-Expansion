#include "Engine/Framework/ApplicationRunOptions.h"

static bool isCommandLineWhitespace(const wide_character character)
{
	return character == L' ' || character == L'\t';
}

static bool tryGetCommandLineArgumentValue(
	const wstring& commandLineText,
	const wstring& argumentKey,
	wstring& argumentValue)
{
	size_t keyPosition = commandLineText.find(argumentKey);
	while (keyPosition != wstring::npos)
	{
		if (keyPosition > 0 && !isCommandLineWhitespace(commandLineText[keyPosition - 1]))
		{
			keyPosition = commandLineText.find(argumentKey, keyPosition + 1);
			continue;
		}

		const size_t valueStart = keyPosition + argumentKey.length();
		if (valueStart >= commandLineText.length())
		{
			return false;
		}

		if (commandLineText[valueStart] == L'"')
		{
			const size_t quotedValueStart = valueStart + 1;
			const size_t quotedValueEnd = commandLineText.find(L'"', quotedValueStart);
			if (quotedValueEnd == wstring::npos)
			{
				return false;
			}

			argumentValue = commandLineText.substr(quotedValueStart, quotedValueEnd - quotedValueStart);
			return true;
		}

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

static bool hasCommandLineArgumentKey(
	const wstring& commandLineText,
	const wstring& argumentKey)
{
	size_t keyPosition = commandLineText.find(argumentKey);
	while (keyPosition != wstring::npos)
	{
		if (keyPosition == 0 || isCommandLineWhitespace(commandLineText[keyPosition - 1]))
		{
			return true;
		}

		keyPosition = commandLineText.find(argumentKey, keyPosition + 1);
	}

	return false;
}

static bool parseUnsignedIntegerText(const wstring& textValue, uint32& parsedValue)
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
		if (value > static_cast<unsigned long long>(uint32MaxValue))
		{
			return false;
		}
	}

	parsedValue = static_cast<uint32>(value);
	return true;
}

static bool parseBackendTypeText(const wstring& backendTypeText, RenderBackendType& backendType)
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

static bool parseBackendValidationInjectModeText(
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

ApplicationRunOptions parseApplicationRunOptions(const WideStringPointer commandLine)
{
	ApplicationRunOptions applicationRunOptions = {};
	const wstring commandLineText = commandLine != nullptr ? commandLine : L"";
	wstring argumentValue = {};

	if (tryGetCommandLineArgumentValue(commandLineText, L"-backend_api=", argumentValue))
	{
		RenderBackendType parsedBackendType = RenderBackendType::dx12;
		if (parseBackendTypeText(argumentValue, parsedBackendType))
		{
			applicationRunOptions.backendType = parsedBackendType;
		}
	}

	if (tryGetCommandLineArgumentValue(commandLineText, L"-backend_debug=", argumentValue))
	{
		uint32 parsedDebugFlag = 0;
		if (parseUnsignedIntegerText(argumentValue, parsedDebugFlag))
		{
			applicationRunOptions.enableBackendDebugLayer = parsedDebugFlag != 0;
		}
	}

	if (tryGetCommandLineArgumentValue(commandLineText, L"-backend_validation_inject=", argumentValue))
	{
		BackendValidationInjectMode parsedInjectMode = BackendValidationInjectMode::none;
		const bool validInjectMode = parseBackendValidationInjectModeText(argumentValue, parsedInjectMode);
		assert(validInjectMode && "[ApplicationRunOptions][Assert] reason=backend_validation_inject_mode_invalid");
		applicationRunOptions.backendValidationInjectMode = parsedInjectMode;
	}

	if (hasCommandLineArgumentKey(commandLineText, L"-perfetto_startup="))
	{
		const bool parsedPerfettoStartupArgument = tryGetCommandLineArgumentValue(commandLineText, L"-perfetto_startup=", argumentValue);
		assert(parsedPerfettoStartupArgument && "[ApplicationRunOptions][Assert] reason=perfetto_startup_argument_malformed");
		uint32 perfettoStartupFlag = 0;
		const bool validPerfettoStartupFlag = parseUnsignedIntegerText(argumentValue, perfettoStartupFlag);
		assert(validPerfettoStartupFlag && "[ApplicationRunOptions][Assert] reason=perfetto_startup_flag_invalid");
		applicationRunOptions.perfettoStartupCapture.enabled = perfettoStartupFlag != 0;
	}

	if (hasCommandLineArgumentKey(commandLineText, L"-perfetto_frames="))
	{
		const bool parsedPerfettoFrameArgument = tryGetCommandLineArgumentValue(commandLineText, L"-perfetto_frames=", argumentValue);
		assert(parsedPerfettoFrameArgument && "[ApplicationRunOptions][Assert] reason=perfetto_frame_argument_malformed");
		uint32 perfettoFrameCount = 0;
		const bool validPerfettoFrameCount = parseUnsignedIntegerText(argumentValue, perfettoFrameCount) && perfettoFrameCount > 0;
		assert(validPerfettoFrameCount && "[ApplicationRunOptions][Assert] reason=perfetto_frame_count_invalid");
		applicationRunOptions.perfettoStartupCapture.stopAfterFrameCount = perfettoFrameCount;
	}

	if (hasCommandLineArgumentKey(commandLineText, L"-perfetto_output="))
	{
		const bool parsedPerfettoOutputArgument = tryGetCommandLineArgumentValue(commandLineText, L"-perfetto_output=", argumentValue);
		assert(parsedPerfettoOutputArgument && "[ApplicationRunOptions][Assert] reason=perfetto_output_argument_malformed");
		applicationRunOptions.perfettoStartupCapture.outputFilePath = filesystem_path(argumentValue).string();
		assert(!applicationRunOptions.perfettoStartupCapture.outputFilePath.empty() && "[ApplicationRunOptions][Assert] reason=perfetto_output_path_invalid");
	}

	return applicationRunOptions;
}
