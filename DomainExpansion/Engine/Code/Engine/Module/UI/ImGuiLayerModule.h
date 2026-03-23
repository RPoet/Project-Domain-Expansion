#pragma once

#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Module/Module.h"
#include "Engine/Platform/PlatformDefine.h"

class CommandList;
class CLIModule;
class Framework;
class PipelineStateObject;
class RenderBackend;
class RootSignatureObject;
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
	void buildAndRender(CommandList* commandList, const float4x4* editorViewProjectionMatrix = nullptr);

private:
	class ImportPanel
	{
	public:
		enum class ProcessCode : int32
		{
			succeeded = 0,
			cliParseFailed = -1,
			cliCommandNotRegistered = -2,
			unsupportedSourceExtension = -3,
			importMissingPath = -100,
			importParseFailed = -101,
			importFbxNotImplemented = -102,
			importUnsupportedExtension = -103,
			importFileOpenFailed = -104,
		};

		explicit ImportPanel(const filesystem_path& filePath);
		void build();
		bool isOpened() const;

	private:
		static string buildFileExtension(const filesystem_path& filePath);
		static string buildFormatText(const filesystem_path& filePath);
		static ProcessCode mapProcessCodeFromCLIExecutionCode(int32 executionCode);
		void executeImportCommand();

		bool opened = false;
		filesystem_path sourceFilePath;
		string sourceFilePathText = {};
		string sourceFileExtension = {};
		string formatText = {};
		string commandText = {};
		ProcessCode processCode = ProcessCode::succeeded;
		bool processCodeAvailable = false;
	};

	struct EditorGridRenderResources
	{
		RootSignatureObject* rootSignatureObject = nullptr;
		PipelineStateObject* pipelineStateObject = nullptr;
	};

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
	void renderEditorGrid(CommandList* commandList, const float4x4& editorViewProjectionMatrix);
	bool resolveEditorGridRenderResources(EditorGridRenderResources& outRenderResources) const;
	void buildOutlinerPanel(World* world);
	void drawOutlinerEntityNode(const World* world, uint32 entityIndex);
	void buildDetailPanel(World* world);
	void buildFileSystemPanel();
	void buildImportPanel();
	void drawDirectoryEntriesRecursive(const filesystem_path& directoryPath);
	bool isImportSupportedFile(const filesystem_path& filePath) const;
	void drawFileEntryContextMenu(const filesystem_path& filePath);
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
	string imguiIniFilePath = {};
	string createWorldNameText = "NewWorld";
	string lastOpenedWorldPath = {};
	string lastEditorActionStatus = {};
	unique_pointer<ImportPanel> importPanel;
};
