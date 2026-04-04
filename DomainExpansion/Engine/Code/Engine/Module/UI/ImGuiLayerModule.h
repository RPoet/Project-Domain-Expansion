#pragma once

#include "Engine/Common/XML/XML.h"
#include "Engine/Framework/FrameworkConstants.h"
#include "Engine/Module/Module.h"
#include "Engine/Platform/PlatformDefine.h"

class CommandList;
class CLIModule;
struct FramePerformanceMetrics;
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
	enum class AnchoredPanelSlot : uint32
	{
		left = 0,
		right = 1,
		bottomLeft = 2,
		bottomRight = 3,
	};

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
		void drawEntityNode(ImGuiLayerModule& owner, World* world, uint32 entityIndex, bool immutableTree);
	};

	class DetailPanel final : public Panel
	{
	public:
		void reset() override;
		void build(ImGuiLayerModule& owner, World* world) override;

	private:
		bool ensureMeshAssetPathsLoaded();
		bool ensureMaterialAssetPathsLoaded();
		void collectMeshAssetPaths(const filesystem_path& directoryPath);
		void collectMaterialAssetPaths(const filesystem_path& directoryPath);

		string meshAssetRootPath = {};
		string materialAssetRootPath = {};
		vector<string> meshAssetPaths = {};
		vector<string> materialAssetPaths = {};
		unordered_map<string, bool> materialAssetDirtyStateByPath = {};
		string createMaterialAssetNameText = "NewMaterial";
		string createMaterialAssetStatusText = {};
		bool meshAssetPathsLoaded = false;
		bool materialAssetPathsLoaded = false;
		int32 focusedMaterialSectionIndex = 0;
		bool showAllMaterialSections = false;
	};

	class DeassetViewerPanel final : public Panel
	{
	public:
		void reset() override;
		void open(const filesystem_path& filePath, const string& displayPathText);
		void build(ImGuiLayerModule& owner, World* world) override;

	private:
		static const char* buildParseCodeText(XML::ParseCode parseCode);
		bool reloadDocument();
		bool saveDocument();
		void recordSavedDocumentChanges(ImGuiLayerModule& owner);
		void rebuildDocumentKeys();

		bool opened = false;
		bool dirty = false;
		string absoluteFilePath = {};
		string displayPathText = {};
		XMLKeyValueDocument document = {};
		XMLKeyValueDocument loadedDocument = {};
		vector<string> documentKeys = {};
		string statusText = {};
	};

	class FileSystemPanel final : public Panel
	{
	public:
		explicit FileSystemPanel(ImportPanel& importPanelReference, DeassetViewerPanel& deassetViewerPanelReference)
			: importPanel(importPanelReference)
			, deassetViewerPanel(deassetViewerPanelReference)
		{
		}

		void reset() override;
		void build(ImGuiLayerModule& owner, World* world) override;

	private:
		bool createWorldFile(ImGuiLayerModule& owner, const string& requestedWorldName, string& outWorldFilePath);
		bool ensureImportSupportedExtensionsLoaded();
		bool resolveResourcesRootPath();
		string buildResourceAssetPath(const filesystem_path& filePath) const;
		bool isDeassetDocumentFile(const filesystem_path& filePath) const;
		bool isWorldAssetFile(const filesystem_path& filePath) const;
		void drawDirectoryEntriesRecursive(ImGuiLayerModule& owner, const filesystem_path& directoryPath);
		void drawFileEntryContextMenu(ImGuiLayerModule& owner, const filesystem_path& filePath);
		bool isImportSupportedFile(const filesystem_path& filePath);

		ImportPanel& importPanel;
		DeassetViewerPanel& deassetViewerPanel;
		string resourcesRootPathText = {};
		bool resourcesRootResolved = false;
		bool resourcesRootValid = false;
		string createWorldNameText = "NewWorld";
		string lastOpenedWorldPath = {};
		vector<string> supportedImportExtensions = {};
		bool supportedImportExtensionsLoaded = false;
		bool collapsed = false;
	};

	class PerformanceMonitorPanel final : public Panel
	{
	public:
		void reset() override;
		void build(ImGuiLayerModule& owner, World* world) override;

	private:
		void pushSample(uint64 worldUpdateSerial, const FramePerformanceMetrics& framePerformanceMetrics);

		constexpr static uint32 historySampleCapacity = 180;
		float worldCpuFrameTimeHistory[historySampleCapacity] = {};
		float renderCommandCpuFrameTimeHistory[historySampleCapacity] = {};
		float gpuFrameTimeHistory[historySampleCapacity] = {};
		uint32 historyWriteIndex = 0;
		uint32 historySampleCount = 0;
		uint64 lastRecordedWorldUpdateSerial = 0;
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
	void applyAnchoredPanelLayout(AnchoredPanelSlot panelSlot, bool bottomPanelCollapsed = false) const;
	float calculateUiScale() const;
	void renderEditorGrid(CommandList* commandList, const float4x4& editorViewProjectionMatrix);
	bool resolveEditorGridRenderResources(EditorGridRenderResources& outRenderResources) const;
	bool tryDeleteSelectedEntity(World* world);
	bool tryReparentEntity(World* world, uint32 childEntityIndex, uint32 parentEntityIndex);
	bool saveActiveWorldImmediate();
	bool recordEditorReplayCommand(const string& commandName, const vector<string>& arguments) const;
	bool recordEditorReplayCommandText(const string& commandText) const;

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
	unique_pointer<DeassetViewerPanel> deassetViewerPanel;
	unique_pointer<PerformanceMonitorPanel> performanceMonitorPanel;
	unique_pointer<FileSystemPanel> fileSystemPanel;
	vector<Panel*> panels = {};
};
