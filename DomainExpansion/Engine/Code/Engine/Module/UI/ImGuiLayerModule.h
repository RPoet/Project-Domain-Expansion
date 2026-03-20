#pragma once

#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Module/Module.h"
#include "Engine/Platform/PlatformDefine.h"

class CommandList;
class Framework;
class RenderBackend;
class World;

class ImGuiLayerModule final : public StaticModule<ImGuiLayerModule>
{
public:
	ImGuiLayerModule()
		: StaticModule("ImGuiLayerModule")
	{
	}
	~ImGuiLayerModule() override;

	bool init(Framework& framework) override;
	void preUpdate() override;
	void postUpdate() override;
	void shutdown() override;

	bool processNativeMessage(
		HandleWindow windowHandle,
		MessageIdentifier messageIdentifier,
		MessageFirstParameter firstParameter,
		MessageSecondParameter secondParameter);
	bool isEditorInputReady() const;
	bool wantsTextInput() const;
	bool wantsMouseCapture() const;
	void buildAndRender(CommandList* commandList);

private:
	struct BackendBridge
	{
		virtual ~BackendBridge() = default;
		virtual bool initialize(RenderBackend& renderBackend) = 0;
		virtual void shutdown() = 0;
		virtual bool beginFrame() = 0;
		virtual bool renderDrawData(CommandList* commandList) = 0;
	};
	struct Dx12BackendBridge;

	bool initializeContext();
	void shutdownContext();
	void updateUiScaleIfNeeded();
	float calculateUiScale() const;
	void buildOutlinerPanel(World* world);
	void drawOutlinerEntityNode(const World* world, uint32 entityIndex);
	void buildDetailPanel(World* world);
	void buildFileSystemPanel();
	void drawDirectoryEntriesRecursive(const filesystem_path& directoryPath);
	bool tryDeleteSelectedEntity(World* world);
	bool createWorldFile(const string& requestedWorldName, string& outWorldFilePath);
	bool saveActiveWorldImmediate();
	bool resolveResourcesRootPath();

	Framework* frameworkReference = nullptr;
	unique_pointer<BackendBridge> backendBridge;
	bool contextCreated = false;
	bool win32BackendInitialized = false;
	uint32 selectedEntityIndex = invalidEntityIndex;
	string resourcesRootPathText;
	bool resourcesRootResolved = false;
	bool resourcesRootValid = false;
	float currentUiScale = 1.0f;
	bool uiScaleInitialized = false;
	string createWorldNameText = "NewWorld";
	string lastOpenedWorldPath = {};
	string lastEditorActionStatus = {};
};
