#pragma once

#include "Engine/Platform/PlatformDefine.h"
#include "Render/Backends/RenderBackend.h"

enum class BackendValidationSeverity : uint32
{
	info = 0,
	warning = 1,
	error = 2,
	corruption = 3,
};

enum class BackendValidationInjectMode : uint32
{
	none = 0,
	warning = 1,
	error = 2,
};

struct BackendValidationMessage
{
	BackendValidationSeverity severity = BackendValidationSeverity::info;
	string id = {};
	string text = {};
};

struct BackendValidationPollResult
{
	vector<BackendValidationMessage> messages = {};
	uint32 totalCount = 0;
	uint32 failingCount = 0;
};

bool isBackendValidationFailingSeverity(BackendValidationSeverity severity);
const char* getBackendValidationSeverityText(BackendValidationSeverity severity);

BackendValidationPollResult collectDx12Messages(RenderBackend* renderBackend);
BackendValidationPollResult collectVulkanMessagesStub();
BackendValidationPollResult collectMetalMessagesStub();
BackendValidationPollResult collectBackendMessages(
	RenderBackendType backendType,
	RenderBackend* renderBackend,
	BackendValidationInjectMode injectMode);
