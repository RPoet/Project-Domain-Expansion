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
	ImGuiLayerModule();
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
	class Panel
	{
	public:
		virtual ~Panel() = default;
		virtual void reset()
		{
		}

		virtual void build(ImGuiLayerModule& owner, World* world) = 0;
	};

	class ImportPanel final : public Panel
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

		void reset() override;
		void open(const filesystem_path& filePath);
		void build(ImGuiLayerModule& owner, World* world) override;

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

	class OutlinerPanel final : public Panel
	{
	public:
		void build(ImGuiLayerModule& owner, World* world) override;

	private:
		void drawEntityNode(ImGuiLayerModule& owner, const World* world, uint32 entityIndex);
	};

	class DetailPanel final : public Panel
	{
	public:
		void build(ImGuiLayerModule& owner, World* world) override;
	};

	class FileSystemPanel final : public Panel
	{
	public:
		explicit FileSystemPanel(ImportPanel& importPanelReference)
			: importPanel(importPanelReference)
		{
		}

		void reset() override;
		void build(ImGuiLayerModule& owner, World* world) override;

	private:
		bool createWorldFile(const string& requestedWorldName, string& outWorldFilePath);
		bool resolveResourcesRootPath();
		void drawDirectoryEntriesRecursive(ImGuiLayerModule& owner, const filesystem_path& directoryPath);
		void drawFileEntryContextMenu(const filesystem_path& filePath);
		bool isImportSupportedFile(const filesystem_path& filePath) const;

		ImportPanel& importPanel;
		string resourcesRootPathText = {};
		bool resourcesRootResolved = false;
		bool resourcesRootValid = false;
		string createWorldNameText = "NewWorld";
		string lastOpenedWorldPath = {};
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
	bool tryDeleteSelectedEntity(World* world);
	bool saveActiveWorldImmediate();

	Framework* frameworkReference = nullptr;
	unique_pointer<BackendBridge> backendBridge;
	bool contextCreated = false;
	bool win32BackendInitialized = false;
	uint32 selectedEntityIndex = invalidEntityIndex;
	float currentUiScale = 1.0f;
	bool uiScaleInitialized = false;
	string imguiIniFilePath = {};
	string lastEditorActionStatus = {};
	unique_pointer<ImportPanel> importPanel;
	unique_pointer<OutlinerPanel> outlinerPanel;
	unique_pointer<DetailPanel> detailPanel;
	unique_pointer<FileSystemPanel> fileSystemPanel;
	vector<Panel*> panels = {};
};
