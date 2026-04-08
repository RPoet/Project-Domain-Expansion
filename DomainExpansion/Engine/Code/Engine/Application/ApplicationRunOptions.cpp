#include "Engine/Application/ApplicationRunOptions.h"
#include "Engine/Common/StringSlice.h"

static bool isApplicationRunOptionWhitespace(const wide_character character)
{
	return character == L' ' || character == L'\t';
}

static bool tryGetApplicationRunOptionValue(
	const wstring& commandLineText,
	const wstring& argumentKey,
	wstring& argumentValue)
{
	size_t keyPosition = commandLineText.find(argumentKey);
	while (keyPosition != wstring::npos)
	{
		if (keyPosition > 0 && !isApplicationRunOptionWhitespace(commandLineText[keyPosition - 1]))
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

			argumentValue = sliceString(commandLineText, quotedValueStart, quotedValueEnd - quotedValueStart);
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

		argumentValue = sliceString(commandLineText, valueStart, valueEnd - valueStart);
		return true;
	}

	return false;
}

static bool hasApplicationRunOptionKey(
	const wstring& commandLineText,
	const wstring& argumentKey)
{
	size_t keyPosition = commandLineText.find(argumentKey);
	while (keyPosition != wstring::npos)
	{
		if (keyPosition == 0 || isApplicationRunOptionWhitespace(commandLineText[keyPosition - 1]))
		{
			return true;
		}

		keyPosition = commandLineText.find(argumentKey, keyPosition + 1);
	}

	return false;
}

static bool parseApplicationRunOptionUnsignedInteger(const wstring& textValue, uint32& parsedValue)
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

static bool parseApplicationRunBackendType(const wstring& backendTypeText, RenderBackendType& backendType)
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

static bool parseApplicationRunBackendValidationInjectMode(
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

static bool parseApplicationRunProfilerBackendType(
	const wstring& backendTypeText,
	ProfilerBackendType& backendType)
{
	if (backendTypeText == L"none")
	{
		backendType = ProfilerBackendType::none;
		return true;
	}

	if (backendTypeText == L"perfetto")
	{
		backendType = ProfilerBackendType::perfetto;
		return true;
	}

	return false;
}

ApplicationRunOptions parseApplicationRunOptions(const WideStringPointer commandLine)
{
	ApplicationRunOptions applicationRunOptions = {};

	const wstring commandLineText = commandLine != nullptr ? commandLine : L"";
	wstring argumentValue = {};

	if (tryGetApplicationRunOptionValue(commandLineText, L"-backend_api=", argumentValue))
	{
		RenderBackendType parsedBackendType = RenderBackendType::dx12;
		if (parseApplicationRunBackendType(argumentValue, parsedBackendType))
		{
			applicationRunOptions.backendType = parsedBackendType;
		}
	}

	if (tryGetApplicationRunOptionValue(commandLineText, L"-backend_debug=", argumentValue))
	{
		uint32 parsedDebugFlag = 0;
		if (parseApplicationRunOptionUnsignedInteger(argumentValue, parsedDebugFlag))
		{
			applicationRunOptions.enableBackendDebugLayer = parsedDebugFlag != 0;
		}
	}

	if (tryGetApplicationRunOptionValue(commandLineText, L"-backend_validation_inject=", argumentValue))
	{
		BackendValidationInjectMode parsedInjectMode = BackendValidationInjectMode::none;
		const bool validInjectMode = parseApplicationRunBackendValidationInjectMode(argumentValue, parsedInjectMode);
		assert(validInjectMode && "[ApplicationRunOptions][Assert] reason=backend_validation_inject_mode_invalid");
		applicationRunOptions.backendValidationInjectMode = parsedInjectMode;
	}

	if (tryGetApplicationRunOptionValue(commandLineText, L"-profiler_backend=", argumentValue))
	{
		ProfilerBackendType parsedBackendType = ProfilerBackendType::none;
		const bool validBackendType = parseApplicationRunProfilerBackendType(argumentValue, parsedBackendType);
		assert(validBackendType && "[ApplicationRunOptions][Assert] reason=profiler_backend_invalid");
		applicationRunOptions.profilerBackendType = parsedBackendType;
	}

	if (hasApplicationRunOptionKey(commandLineText, L"-profiler_output="))
	{
		const bool parsedProfilerOutput = tryGetApplicationRunOptionValue(commandLineText, L"-profiler_output=", argumentValue);
		assert(parsedProfilerOutput && "[ApplicationRunOptions][Assert] reason=profiler_output_argument_malformed");
		applicationRunOptions.profilerCaptureOutputFilePath = filesystem_path(argumentValue).lexically_normal().string();
	}

	if (hasApplicationRunOptionKey(commandLineText, L"-quit_after_frames="))
	{
		const bool parsedQuitAfterFrames = tryGetApplicationRunOptionValue(commandLineText, L"-quit_after_frames=", argumentValue);
		assert(parsedQuitAfterFrames && "[ApplicationRunOptions][Assert] reason=quit_after_frames_argument_malformed");
		uint32 parsedFrameCount = 0;
		if (parseApplicationRunOptionUnsignedInteger(argumentValue, parsedFrameCount))
		{
			applicationRunOptions.quitAfterFrameCount = parsedFrameCount;
		}
	}

	return applicationRunOptions;
}
