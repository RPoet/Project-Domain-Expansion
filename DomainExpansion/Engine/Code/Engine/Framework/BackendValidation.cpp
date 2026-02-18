#include "Engine/Framework/BackendValidation.h"

#include <d3d12.h>
#include <d3d12sdklayers.h>

static BackendValidationSeverity mapDx12Severity(const D3D12_MESSAGE_SEVERITY severity)
{
	switch (severity)
	{
	case D3D12_MESSAGE_SEVERITY_CORRUPTION:
		return BackendValidationSeverity::corruption;
	case D3D12_MESSAGE_SEVERITY_ERROR:
		return BackendValidationSeverity::error;
	case D3D12_MESSAGE_SEVERITY_WARNING:
		return BackendValidationSeverity::warning;
	case D3D12_MESSAGE_SEVERITY_INFO:
	case D3D12_MESSAGE_SEVERITY_MESSAGE:
	default:
		return BackendValidationSeverity::info;
	}
}

bool isBackendValidationFailingSeverity(const BackendValidationSeverity severity)
{
	return severity == BackendValidationSeverity::warning
		|| severity == BackendValidationSeverity::error
		|| severity == BackendValidationSeverity::corruption;
}

const char* getBackendValidationSeverityText(const BackendValidationSeverity severity)
{
	switch (severity)
	{
	case BackendValidationSeverity::warning:
		return "warning";
	case BackendValidationSeverity::error:
		return "error";
	case BackendValidationSeverity::corruption:
		return "corruption";
	case BackendValidationSeverity::info:
	default:
		return "info";
	}
}

static void appendBackendValidationMessage(
	BackendValidationPollResult& pollResult,
	const BackendValidationSeverity severity,
	const string& id,
	const string& text)
{
	BackendValidationMessage message = {};
	message.severity = severity;
	message.id = id;
	message.text = text;
	pollResult.messages.push_back(moveValue(message));

	++pollResult.totalCount;
	if (isBackendValidationFailingSeverity(severity))
	{
		++pollResult.failingCount;
	}
}

BackendValidationPollResult collectDx12Messages(RenderBackend* renderBackend)
{
	BackendValidationPollResult pollResult = {};
	if (renderBackend == nullptr)
	{
		return pollResult;
	}

	ID3D12Device* dx12Device = static_cast<ID3D12Device*>(renderBackend->getNativeGraphicsDevice());
	if (dx12Device == nullptr)
	{
		return pollResult;
	}

	com_pointer<ID3D12InfoQueue> infoQueue;
	if (FAILED(dx12Device->QueryInterface(IID_PPV_ARGS(&infoQueue))) || infoQueue == nullptr)
	{
		return pollResult;
	}

	const uint64 messageCount =
		static_cast<uint64>(infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter());
	for (uint64 messageIndex = 0; messageIndex < messageCount; ++messageIndex)
	{
		SIZE_T messageByteCount = 0;
		if (FAILED(infoQueue->GetMessage(static_cast<SIZE_T>(messageIndex), nullptr, &messageByteCount))
			|| messageByteCount == 0)
		{
			continue;
		}

		vector<char> messageStorage(messageByteCount);
		D3D12_MESSAGE* dx12Message = reinterpret_cast<D3D12_MESSAGE*>(messageStorage.data());
		if (FAILED(infoQueue->GetMessage(static_cast<SIZE_T>(messageIndex), dx12Message, &messageByteCount))
			|| dx12Message == nullptr)
		{
			continue;
		}

		appendBackendValidationMessage(
			pollResult,
			mapDx12Severity(dx12Message->Severity),
			std::to_string(static_cast<uint32>(dx12Message->ID)),
			dx12Message->pDescription != nullptr ? dx12Message->pDescription : "no_description");
	}

	infoQueue->ClearStoredMessages();
	return pollResult;
}

BackendValidationPollResult collectVulkanMessagesStub()
{
	BackendValidationPollResult pollResult = {};
	appendBackendValidationMessage(
		pollResult,
		BackendValidationSeverity::warning,
		"validator_not_implemented",
		"vulkan validation collector is not implemented");
	return pollResult;
}

BackendValidationPollResult collectMetalMessagesStub()
{
	BackendValidationPollResult pollResult = {};
	appendBackendValidationMessage(
		pollResult,
		BackendValidationSeverity::warning,
		"validator_not_implemented",
		"metal validation collector is not implemented");
	return pollResult;
}

static void injectSyntheticValidationMessage(
	BackendValidationPollResult& pollResult,
	const BackendValidationInjectMode injectMode)
{
	if (injectMode == BackendValidationInjectMode::none)
	{
		return;
	}

	if (injectMode == BackendValidationInjectMode::warning)
	{
		appendBackendValidationMessage(
			pollResult,
			BackendValidationSeverity::warning,
			"synthetic_warning",
			"synthetic backend validation warning injected");
		return;
	}

	appendBackendValidationMessage(
		pollResult,
		BackendValidationSeverity::error,
		"synthetic_error",
		"synthetic backend validation error injected");
}

BackendValidationPollResult collectBackendMessages(
	const RenderBackendType backendType,
	RenderBackend* renderBackend,
	const BackendValidationInjectMode injectMode)
{
	BackendValidationPollResult pollResult = {};
	switch (backendType)
	{
	case RenderBackendType::dx12:
		pollResult = collectDx12Messages(renderBackend);
		break;
	case RenderBackendType::vulkan:
		pollResult = collectVulkanMessagesStub();
		break;
	case RenderBackendType::metal:
		pollResult = collectMetalMessagesStub();
		break;
	default:
		appendBackendValidationMessage(
			pollResult,
			BackendValidationSeverity::warning,
			"backend_type_unknown",
			"backend validation collector is not available for this backend");
		break;
	}

	injectSyntheticValidationMessage(pollResult, injectMode);
	return pollResult;
}
