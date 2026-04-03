#include "Engine/Module/UI/ImGuiLayerModule.h"

#include "Engine/Assets/AssetLoader.h"
#include "Engine/Common/EditorCommandCommon.h"
#include "Engine/Common/EditorCommandReplay.h"
#include "Engine/Framework/Entity.h"
#include "Engine/Framework/CameraComponent.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/World.h"
#include "Engine/Module/MeshParser/MeshParser.h"
#include "Engine/Module/MeshStreaming/MeshStreaming.h"
#include "Engine/Module/CLI/CLIModule.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/ShaderPackage/ShaderPackageModule.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Module/TextureParser/TextureParser.h"
#include "Render/Backends/Dx12/Dx12CommandList.h"
#include "Render/Backends/RenderBackend.h"

#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/backends/imgui_impl_dx12.h"
#include "ThirdParty/ImGui/backends/imgui_impl_win32.h"
#include "ThirdParty/ImGui/misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <d3d12.h>
#include <system_error>

static constexpr int32 dx12FrameBufferCountForImGui = 2;
static constexpr const char* outlinerEntityDragDropPayloadType = "OutlinerEntityIndex";
extern IMGUI_IMPL_API MessageResult ImGui_ImplWin32_WndProcHandler(
	HandleWindow windowHandle,
	MessageIdentifier messageIdentifier,
	MessageFirstParameter firstParameter,
	MessageSecondParameter secondParameter);

static bool isDirectoryEntry(const filesystem_directory_entry& directoryEntry)
{
	error_code errorCode;
	return directoryEntry.is_directory(errorCode) && !errorCode;
}

static void sortDirectoryEntries(vector<filesystem_directory_entry>& directoryEntries)
{
	std::sort(
		directoryEntries.begin(),
		directoryEntries.end(),
		[](const filesystem_directory_entry& leftEntry, const filesystem_directory_entry& rightEntry)
		{
			const bool leftIsDirectory = isDirectoryEntry(leftEntry);
			const bool rightIsDirectory = isDirectoryEntry(rightEntry);
			if (leftIsDirectory != rightIsDirectory)
			{
				return leftIsDirectory && !rightIsDirectory;
			}

			return leftEntry.path().filename().string() < rightEntry.path().filename().string();
		});
}

static float clampCameraFieldOfViewYDegrees(const float fieldOfViewYDegrees)
{
	return std::clamp(fieldOfViewYDegrees, 1.0f, 179.0f);
}

static void clampCameraPlanes(float& nearPlane, float& farPlane)
{
	nearPlane = std::max(nearPlane, 0.001f);
	farPlane = std::max(farPlane, nearPlane + 0.001f);
}

static const CameraComponent* getFirstCameraComponent(const World* world, const Entity* entity)
{
	if (world == nullptr || entity == nullptr)
	{
		return nullptr;
	}

	for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
	{
		const uint32 componentIndex = entity->getComponentIndex(componentArrayIndex);
		const Component* component = world->getComponentByIndex(componentIndex);
		if (component == nullptr || component->getComponentType() != CameraComponent::staticComponentType)
		{
			continue;
		}

		return static_cast<const CameraComponent*>(component);
	}

	return nullptr;
}

static bool isEditorCameraEntity(const World* world, const Entity* entity)
{
	const CameraComponent* cameraComponent = getFirstCameraComponent(world, entity);
	return cameraComponent != nullptr && cameraComponent->editorCamera;
}

static bool isImmutableEntity(const World* world, const Entity* entity)
{
	return isEditorCameraEntity(world, entity);
}

static string buildEntityDisplayText(const World* world, const Entity* entity)
{
	const char* entityTypeText = isEditorCameraEntity(world, entity) ? "EditorCamera" : "Entity";
	string displayText = entity != nullptr && !entity->getName().empty() ? entity->getName() : entityTypeText;
	displayText += "(";
	displayText += entityTypeText;
	displayText += ")";

	return displayText;
}

static string buildReplayBooleanArgument(const bool value)
{
	return value ? "1" : "0";
}

static string buildReplayFloatArgument(const float value)
{
	return to_string(value);
}

static string buildReplayUnsignedArgument(const uint32 value)
{
	return to_string(value);
}

static vector<string> buildReplayMeshComponentArguments(const MeshComponent& meshComponent)
{
	vector<string> arguments = {};
	arguments.push_back(meshComponent.getAssetPath());
	arguments.push_back(meshComponent.getMeshAssetPath());
	arguments.push_back(buildReplayUnsignedArgument(meshComponent.getLODLevel()));
	arguments.push_back(buildReplayBooleanArgument(meshComponent.isVisible()));

	const vector<string>& materialAssetPaths = meshComponent.getMaterialAssetPaths();
	arguments.reserve(arguments.size() + materialAssetPaths.size());
	for (uint32 materialIndex = 0; materialIndex < static_cast<uint32>(materialAssetPaths.size()); ++materialIndex)
	{
		arguments.push_back(materialAssetPaths[materialIndex]);
	}

	return arguments;
}

static vector<string> buildReplayMaterialShaderConfigArguments(
	const string& materialAssetPath,
	const string& shaderTemplatePath,
	const string& shaderPackagePath,
	const string& shaderVariantName,
	const string& vertexShaderInjectedCode,
	const string& pixelShaderInjectedCode)
{
	return
	{
		materialAssetPath,
		shaderTemplatePath,
		shaderPackagePath,
		shaderVariantName,
		vertexShaderInjectedCode,
		pixelShaderInjectedCode,
	};
}

static string buildReplaySetMaterialShaderCommandText(const MaterialAsset& materialAsset)
{
	return EditorCommandReplay::buildCommandText(
		"Editor.setMaterialAsset",
		buildReplayMaterialShaderConfigArguments(
			materialAsset.getAssetPath(),
			materialAsset.getShaderTemplatePath(),
			materialAsset.getShaderPackagePath(),
			materialAsset.getShaderVariantName(),
			materialAsset.getVertexShaderInjectedCode(),
			materialAsset.getPixelShaderInjectedCode()));
}

static string buildReplayCreateMaterialAssetCommandText(
	const string& materialAssetPath,
	const string& materialAssetName,
	const string& shaderTemplatePath,
	const string& shaderPackagePath,
	const string& shaderVariantName,
	const string& vertexShaderInjectedCode,
	const string& pixelShaderInjectedCode)
{
	vector<string> arguments = { materialAssetPath, materialAssetName };
	const vector<string> materialShaderConfigArguments = buildReplayMaterialShaderConfigArguments(
		materialAssetPath,
		shaderTemplatePath,
		shaderPackagePath,
		shaderVariantName,
		vertexShaderInjectedCode,
		pixelShaderInjectedCode);
	arguments.insert(arguments.end(), materialShaderConfigArguments.begin() + 1, materialShaderConfigArguments.end());
	return EditorCommandReplay::buildCommandText("Editor.createMaterialAsset", arguments);
}

static Component* findFirstComponentByType(World* world, Entity* entity, const ComponentType componentType)
{
	if (world == nullptr || entity == nullptr || !componentType.isValid())
	{
		return nullptr;
	}

	for (uint32 componentArrayIndex = 0; componentArrayIndex < entity->getComponentCount(); ++componentArrayIndex)
	{
		const uint32 componentIndex = entity->getComponentIndex(componentArrayIndex);
		Component* component = world->getComponentByIndex(componentIndex);
		if (component != nullptr && component->getComponentType() == componentType)
		{
			return component;
		}
	}

	return nullptr;
}

static float clampFloat(const float value, const float minValue, const float maxValue)
{
	if (value < minValue)
	{
		return minValue;
	}

	if (value > maxValue)
	{
		return maxValue;
	}

	return value;
}

struct EditorGridPushConstantData
{
	float4x4 viewProjection = {};
	float minorLineColor[4] = {};
	float majorLineColor[4] = {};
	float axisXColor[4] = {};
	float axisZColor[4] = {};
	float gridParameters[4] = {};
};

static_assert((sizeof(EditorGridPushConstantData) & 3u) == 0, "[ImGuiLayerModule][Assert] reason=grid_push_constant_alignment_invalid");

struct ImGuiLayerModule::Dx12BackendBridge final : ImGuiLayerModule::BackendBridge
{
	bool initialize(RenderBackend& renderBackend) override
	{
		if (initialized)
		{
			return true;
		}

		ID3D12Device* device = static_cast<ID3D12Device*>(renderBackend.getNativeGraphicsDevice());
		if (device == nullptr)
		{
			return false;
		}

		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDescription = {};
		descriptorHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descriptorHeapDescription.NumDescriptors = 1;
		descriptorHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		descriptorHeapDescription.NodeMask = 0;
		if (FAILED(device->CreateDescriptorHeap(
			&descriptorHeapDescription,
			IID_PPV_ARGS(&fontDescriptorHeap))))
		{
			return false;
		}

		const D3D12_CPU_DESCRIPTOR_HANDLE fontCpuDescriptorHandle = fontDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		const D3D12_GPU_DESCRIPTOR_HANDLE fontGpuDescriptorHandle = fontDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		if (!ImGui_ImplDX12_Init(
			device,
			dx12FrameBufferCountForImGui,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			fontDescriptorHeap.Get(),
			fontCpuDescriptorHandle,
			fontGpuDescriptorHandle))
		{
			fontDescriptorHeap.Reset();
			return false;
		}

		initialized = true;
		return true;
	}

	void shutdown() override
	{
		if (!initialized)
		{
			return;
		}

		ImGui_ImplDX12_Shutdown();
		fontDescriptorHeap.Reset();
		initialized = false;
	}

	bool beginFrame() override
	{
		if (!initialized)
		{
			return false;
		}

		ImGui_ImplDX12_NewFrame();
		return true;
	}

	bool renderDrawData(CommandList* commandList) override
	{
		if (!initialized || commandList == nullptr)
		{
			return false;
		}

		Dx12CommandList* dx12CommandList = dynamic_cast<Dx12CommandList*>(commandList);
		if (dx12CommandList == nullptr)
		{
			return false;
		}

		ID3D12GraphicsCommandList* nativeCommandList = dx12CommandList->getNativeCommandList();
		if (nativeCommandList == nullptr || fontDescriptorHeap == nullptr)
		{
			return false;
		}

		ID3D12DescriptorHeap* descriptorHeaps[] = { fontDescriptorHeap.Get() };
		nativeCommandList->SetDescriptorHeaps(1, descriptorHeaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), nativeCommandList);
		return true;
	}

	com_pointer<ID3D12DescriptorHeap> fontDescriptorHeap;
	bool initialized = false;
};

ImGuiLayerModule::ImGuiLayerModule()
	: StaticModule("ImGuiLayerModule")
	, importPanel(new ImportPanel())
	, outlinerPanel(new OutlinerPanel())
	, detailPanel(new DetailPanel())
	, deassetViewerPanel(new DeassetViewerPanel())
	, fileSystemPanel(new FileSystemPanel(*importPanel, *deassetViewerPanel))
{
	panels.push_back(outlinerPanel.get());
	panels.push_back(detailPanel.get());
	panels.push_back(fileSystemPanel.get());
	panels.push_back(importPanel.get());
	panels.push_back(deassetViewerPanel.get());
}

void ImGuiLayerModule::ImportPanel::reset()
{
	opened = false;
	sourceFilePath.clear();
	sourceFilePathText.clear();
	sourceFileExtension.clear();
	formatText.clear();
	commandText.clear();
	processCode = ProcessCode::succeeded;
	processCodeAvailable = false;
}

void ImGuiLayerModule::ImportPanel::open(const filesystem_path& filePath)
{
	reset();

	opened = true;
	sourceFilePath = filePath.lexically_normal();
	sourceFilePathText = sourceFilePath.string();
	sourceFileExtension = buildFileExtension(sourceFilePath);
	formatText = buildFormatText(sourceFilePath);
	executeImportCommand();
}

void ImGuiLayerModule::ImportPanel::build(ImGuiLayerModule& owner, World* world)
{
	unused(owner);
	unused(world);

	if (!opened)
	{
		return;
	}

	if (!ImGui::Begin("Import", &opened))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Source: %s", sourceFilePathText.c_str());
	ImGui::Text("Extension: %s", sourceFileExtension.c_str());
	ImGui::Text("Format: %s", formatText.c_str());
	ImGui::Text("Command: %s", commandText.empty() ? "(empty)" : commandText.c_str());
	ImGui::Separator();
	if (processCodeAvailable)
	{
		ImGui::Text("Process: %d", static_cast<int32>(processCode));
	}
	else
	{
		ImGui::TextUnformatted("Process: idle");
	}
	ImGui::TextWrapped("Import execution is routed through CLI command dispatch.");

	ImGui::End();
}

string ImGuiLayerModule::ImportPanel::buildFileExtension(const filesystem_path& filePath)
{
	string extension = filePath.extension().string();
	tolower(extension);
	return extension;
}

string ImGuiLayerModule::ImportPanel::buildFormatText(const filesystem_path& filePath)
{
	string extension = buildFileExtension(filePath);
	if (extension.empty())
	{
		return "Unknown";
	}

	if (!extension.empty() && extension[0] == '.')
	{
		extension.erase(0, 1);
	}
	return extension;
}

[[noreturn]] static void failUnexpectedImportProcessCode()
{
	assert(false && "[ImGuiLayerModule][Assert] reason=import_panel_process_code_unexpected");
}

ImGuiLayerModule::ImportPanel::ProcessCode ImGuiLayerModule::ImportPanel::mapProcessCodeFromCLIExecutionCode(const int32 executionCode)
{
	if (executionCode == static_cast<int32>(CLIModule::ExecutionCode::parseFailed))
	{
		return ProcessCode::cliParseFailed;
	}

	if (executionCode == static_cast<int32>(CLIModule::ExecutionCode::commandNotRegistered))
	{
		return ProcessCode::cliCommandNotRegistered;
	}

	if (executionCode == static_cast<int32>(MeshParser::ImportCLIExecutionCode::succeeded))
	{
		return ProcessCode::succeeded;
	}

	if (executionCode == static_cast<int32>(MeshParser::ImportCLIExecutionCode::missingPath))
	{
		return ProcessCode::importMissingPath;
	}

	if (executionCode == static_cast<int32>(MeshParser::ImportCLIExecutionCode::parseFailed))
	{
		return ProcessCode::importParseFailed;
	}

	if (executionCode == static_cast<int32>(MeshParser::ImportCLIExecutionCode::fbxNotImplemented))
	{
		return ProcessCode::importFbxNotImplemented;
	}

	if (executionCode == static_cast<int32>(MeshParser::ImportCLIExecutionCode::unsupportedExtension))
	{
		return ProcessCode::importUnsupportedExtension;
	}

	if (executionCode == static_cast<int32>(MeshParser::ImportCLIExecutionCode::fileOpenFailed))
	{
		return ProcessCode::importFileOpenFailed;
	}

	failUnexpectedImportProcessCode();
}

void ImGuiLayerModule::ImportPanel::executeImportCommand()
{
	if (sourceFileExtension.empty())
	{
		processCode = ProcessCode::unsupportedSourceExtension;
		processCodeAvailable = true;
		return;
	}

	commandText = (TextureParser::supportsImportExtension(sourceFileExtension) ? "TextureParser.import \"" : "MeshParser.import \"")
		+ sourceFilePathText
		+ "\"";
	CLIModule::execute(commandText);
	shared_pointer<CLIModule> cliModule = CLIModule::get();
	processCode = mapProcessCodeFromCLIExecutionCode(cliModule->getLastExecutionCode());
	processCodeAvailable = true;
	if (processCode == ProcessCode::succeeded)
	{
		if (!EditorCommandReplay::appendCommandText(commandText))
		{
			error << "[EditorReplayLog] action=append_failed command=" << commandText << lineBreak;
		}
	}
}

ImGuiLayerModule::~ImGuiLayerModule() = default;

bool ImGuiLayerModule::init(Framework& framework)
{
	shutdown();

	frameworkReference = &framework;
	selectedEntityIndex = invalidEntityIndex;
	currentUiScale = 1.0f;
	uiScaleInitialized = false;
	lastEditorActionStatus.clear();
	const bool validPanels = panels.size() == 5
		&& importPanel != nullptr
		&& outlinerPanel != nullptr
		&& detailPanel != nullptr
		&& deassetViewerPanel != nullptr
		&& fileSystemPanel != nullptr;
	assert(validPanels && "[ImGuiLayerModule][Assert] reason=imgui_panel_missing");
	for (size_t panelIndex = 0; panelIndex < panels.size(); ++panelIndex)
	{
		panels[panelIndex]->reset();
	}

	if (!framework.isEditorUIEnabled())
	{
		return true;
	}

	if (!initializeContext())
	{
		return false;
	}

	WindowsWindowObject* windowObject = framework.getWindowObject();
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	const bool validWorldFlowPrerequisites =
		windowObject != nullptr
		&& renderBackendModule != nullptr
		&& renderBackendModule->isBackendCreated();
	assert(validWorldFlowPrerequisites && "[ImGuiLayerModule][Assert] reason=worldflow_prerequisite_missing");

	const bool initializedWin32Backend = ImGui_ImplWin32_Init(windowObject->getWindowHandle());
	assert(initializedWin32Backend && "[ImGuiLayerModule][Assert] reason=win32_backend_init_failed");

	win32BackendInitialized = true;

	if (framework.getBackendOptions().backendType != RenderBackendType::dx12)
	{
		output << "[ImGuiLayer][Warn] backend_not_supported_for_imgui" << lineBreak;
		return true;
	}

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	assert(renderBackend != nullptr && "[ImGuiLayerModule][Assert] reason=render_backend_missing");

	backendBridge.reset(new Dx12BackendBridge());
	const bool initializedDx12Backend =
		backendBridge != nullptr
		&& backendBridge->initialize(*renderBackend);
	assert(initializedDx12Backend && "[ImGuiLayerModule][Assert] reason=dx12_backend_init_failed");

	updateUiScaleIfNeeded();
	return true;
}

void ImGuiLayerModule::preUpdate()
{
	if (frameworkReference == nullptr || !frameworkReference->isEditorUIEnabled())
	{
		return;
	}

	updateUiScaleIfNeeded();
}

void ImGuiLayerModule::postUpdate()
{
}

void ImGuiLayerModule::shutdown()
{
	if (backendBridge != nullptr)
	{
		backendBridge->shutdown();
		backendBridge.reset();
	}

	if (win32BackendInitialized)
	{
		ImGui_ImplWin32_Shutdown();
		win32BackendInitialized = false;
	}

	shutdownContext();
	frameworkReference = nullptr;
	selectedEntityIndex = invalidEntityIndex;
	currentUiScale = 1.0f;
	uiScaleInitialized = false;
	lastEditorActionStatus.clear();
	const bool validPanels = panels.size() == 5
		&& importPanel != nullptr
		&& outlinerPanel != nullptr
		&& detailPanel != nullptr
		&& deassetViewerPanel != nullptr
		&& fileSystemPanel != nullptr;
	assert(validPanels && "[ImGuiLayerModule][Assert] reason=imgui_panel_missing");
	for (size_t panelIndex = 0; panelIndex < panels.size(); ++panelIndex)
	{
		panels[panelIndex]->reset();
	}
}

bool ImGuiLayerModule::processNativeMessage(
	HandleWindow windowHandle,
	const MessageIdentifier messageIdentifier,
	const MessageFirstParameter firstParameter,
	const MessageSecondParameter secondParameter)
{
	if (!contextCreated
		|| !win32BackendInitialized
		|| frameworkReference == nullptr
		|| !frameworkReference->isEditorUIEnabled())
	{
		return false;
	}

	return ImGui_ImplWin32_WndProcHandler(
		windowHandle,
		messageIdentifier,
		firstParameter,
		secondParameter) != 0;
}

bool ImGuiLayerModule::isEditorInputReady() const
{
	return contextCreated
		&& frameworkReference != nullptr
		&& frameworkReference->isEditorUIEnabled();
}

bool ImGuiLayerModule::wantsTextInput() const
{
	if (!isEditorInputReady())
	{
		return false;
	}

	return ImGui::GetIO().WantTextInput;
}

bool ImGuiLayerModule::wantsMouseCapture() const
{
	if (!isEditorInputReady())
	{
		return false;
	}

	return ImGui::GetIO().WantCaptureMouse;
}

void ImGuiLayerModule::buildAndRender(
	CommandList* commandList,
	const float4x4* editorViewProjectionMatrix)
{
	if (commandList == nullptr
		|| !contextCreated
		|| !win32BackendInitialized
		|| backendBridge == nullptr
		|| frameworkReference == nullptr
		|| !frameworkReference->isEditorUIEnabled())
	{
		return;
	}

	if (editorViewProjectionMatrix != nullptr)
	{
		renderEditorGrid(commandList, *editorViewProjectionMatrix);
	}

	if (!backendBridge->beginFrame())
	{
		return;
	}

	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	World* world = frameworkReference->getActiveWorld();
	const bool validPanels = panels.size() == 5
		&& importPanel != nullptr
		&& outlinerPanel != nullptr
		&& detailPanel != nullptr
		&& deassetViewerPanel != nullptr
		&& fileSystemPanel != nullptr
		&& panels[0] != nullptr
		&& panels[1] != nullptr
		&& panels[2] != nullptr
		&& panels[3] != nullptr
		&& panels[4] != nullptr;
	assert(validPanels && "[ImGuiLayerModule][Assert] reason=imgui_panel_missing");
	for (size_t panelIndex = 0; panelIndex < panels.size(); ++panelIndex)
	{
		panels[panelIndex]->build(*this, world);
	}

	ImGui::Render();
	backendBridge->renderDrawData(commandList);
}

void ImGuiLayerModule::renderEditorGrid(
	CommandList* commandList,
	const float4x4& editorViewProjectionMatrix)
{
	if (commandList == nullptr)
	{
		return;
	}

	EditorGridRenderResources renderResources = {};
	if (!resolveEditorGridRenderResources(renderResources))
	{
		return;
	}

	EditorGridPushConstantData pushConstantData = {};
	pushConstantData.viewProjection = editorViewProjectionMatrix;

	pushConstantData.minorLineColor[0] = 0.33f;
	pushConstantData.minorLineColor[1] = 0.37f;
	pushConstantData.minorLineColor[2] = 0.42f;
	pushConstantData.minorLineColor[3] = 0.16f;

	pushConstantData.majorLineColor[0] = 0.48f;
	pushConstantData.majorLineColor[1] = 0.53f;
	pushConstantData.majorLineColor[2] = 0.59f;
	pushConstantData.majorLineColor[3] = 0.28f;

	pushConstantData.axisXColor[0] = 0.88f;
	pushConstantData.axisXColor[1] = 0.30f;
	pushConstantData.axisXColor[2] = 0.24f;
	pushConstantData.axisXColor[3] = 0.72f;

	pushConstantData.axisZColor[0] = 0.24f;
	pushConstantData.axisZColor[1] = 0.54f;
	pushConstantData.axisZColor[2] = 0.90f;
	pushConstantData.axisZColor[3] = 0.72f;

	pushConstantData.gridParameters[0] = 1.0f;
	pushConstantData.gridParameters[1] = 10.0f;
	pushConstantData.gridParameters[2] = 256.0f;
	pushConstantData.gridParameters[3] = 48.0f;

	commandList->setPipeline(renderResources.pipelineStateObject, renderResources.rootSignatureObject);
	commandList->setGraphicsPushConstants(0, &pushConstantData, static_cast<uint32>(sizeof(pushConstantData)));
	commandList->setPrimitiveTopology(PrimitiveTopology::triangleList);
	commandList->draw(6, 1, 0, 0);
}

bool ImGuiLayerModule::resolveEditorGridRenderResources(EditorGridRenderResources& outRenderResources) const
{
	outRenderResources = {};

	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	shared_pointer<ShaderPackageModule> shaderPackageModule = ShaderPackageModule::get();
	unused(shaderPackageModule);
	const bool validModules = renderBackendModule->isBackendCreated();
	assert(validModules && "[ImGuiLayerModule][Assert] reason=editor_grid_module_missing");

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	assert(renderBackend != nullptr && "[ImGuiLayerModule][Assert] reason=editor_grid_render_backend_missing");

	shared_pointer<ShaderPackageAsset> shaderPackage = shaderPackageModule->getOrLoadPackage("Shaders/Packages/EditorGrid.shaderpkg");
	const bool shaderPackageReady = shaderPackage != nullptr && shaderPackage->state == ShaderPackageState::ready;
	assert(shaderPackageReady && "[ImGuiLayerModule][Assert] reason=editor_grid_shader_package_not_ready");

	const auto findShaderPackageVariantByName = [](const ShaderPackageAsset& shaderPackageAsset, const string& variantName) -> const ShaderPackageVariant*
	{
		for (uint32 variantIndex = 0; variantIndex < static_cast<uint32>(shaderPackageAsset.variants.size()); ++variantIndex)
		{
			const ShaderPackageVariant& variant = shaderPackageAsset.variants[variantIndex];
			if (variant.name == variantName)
			{
				return &variant;
			}
		}

		return nullptr;
	};

	const ShaderPackageVariant* shaderVariant = findShaderPackageVariantByName(*shaderPackage, "EditorGridDefault");
	assert(shaderVariant != nullptr && "[ImGuiLayerModule][Assert] reason=editor_grid_shader_variant_missing");

	shared_pointer<ShaderObject> vertexShader = shaderVariant->getShader(ShaderStage::vertex);
	shared_pointer<ShaderObject> pixelShader = shaderVariant->getShader(ShaderStage::pixel);
	const bool validShaders = vertexShader != nullptr && pixelShader != nullptr;
	assert(validShaders && "[ImGuiLayerModule][Assert] reason=editor_grid_shader_stage_missing");

	PipelineStateDesc pipelineStateDesc = {};
	pipelineStateDesc.pipelineStateType = PipelineStateType::graphics;
	pipelineStateDesc.vertexShader = vertexShader;
	pipelineStateDesc.pixelShader = pixelShader;
	pipelineStateDesc.sampleCount = 1;

	PushConstantRange pushConstantRange = {};
	pushConstantRange.offsetInBytes = 0;
	pushConstantRange.sizeInBytes = static_cast<uint32>(sizeof(EditorGridPushConstantData));
	pushConstantRange.shaderVisibility = ShaderVisibility::allGraphics;
	pipelineStateDesc.rootSignatureDesc.pushConstantRanges.push_back(pushConstantRange);

	PipelineRenderTargetDesc renderTargetDesc = {};
	renderTargetDesc.colorFormat = TextureFormat::rgba8Unorm;
	renderTargetDesc.blendDesc.blendEnabled = true;
	renderTargetDesc.blendDesc.sourceColorBlendFactor = PipelineBlendFactor::sourceAlpha;
	renderTargetDesc.blendDesc.destinationColorBlendFactor = PipelineBlendFactor::inverseSourceAlpha;
	renderTargetDesc.blendDesc.colorBlendOperation = PipelineBlendOperation::add;
	renderTargetDesc.blendDesc.sourceAlphaBlendFactor = PipelineBlendFactor::one;
	renderTargetDesc.blendDesc.destinationAlphaBlendFactor = PipelineBlendFactor::inverseSourceAlpha;
	renderTargetDesc.blendDesc.alphaBlendOperation = PipelineBlendOperation::add;
	pipelineStateDesc.renderTargets.push_back(renderTargetDesc);

	pipelineStateDesc.depthStencilDesc.depthStencilFormat = TextureFormat::d32Float;
	pipelineStateDesc.depthStencilDesc.depthTestEnabled = true;
	pipelineStateDesc.depthStencilDesc.depthWriteEnabled = false;
	pipelineStateDesc.depthStencilDesc.depthCompareOperation = PipelineCompareOperation::lessEqual;
	pipelineStateDesc.cullMode = PipelineCullMode::none;

	outRenderResources.rootSignatureObject = renderBackend->getOrCreateRootSignatureObject(pipelineStateDesc.rootSignatureDesc);
	outRenderResources.pipelineStateObject = renderBackend->getOrCreatePipelineStateObject(pipelineStateDesc);
	const bool validPipelineObjects = outRenderResources.rootSignatureObject != nullptr
		&& outRenderResources.pipelineStateObject != nullptr;
	assert(validPipelineObjects && "[ImGuiLayerModule][Assert] reason=editor_grid_pipeline_create_failed");
	return validPipelineObjects;
}

bool ImGuiLayerModule::initializeContext()
{
	if (contextCreated)
	{
		return true;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& imguiIo = ImGui::GetIO();
	imguiIo.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	if (diskLoaderModule->TEMP_resolveImGuiIniFilePath(imguiIniFilePath))
	{
		imguiIo.IniFilename = imguiIniFilePath.c_str();
	}
	ImGui::StyleColorsDark();
	contextCreated = true;
	return true;
}

void ImGuiLayerModule::shutdownContext()
{
	if (!contextCreated)
	{
		return;
	}

	ImGui::DestroyContext();
	contextCreated = false;
	imguiIniFilePath.clear();
}

void ImGuiLayerModule::updateUiScaleIfNeeded()
{
	if (!contextCreated)
	{
		return;
	}

	const float targetScale = calculateUiScale();
	const float scaleDelta = targetScale - currentUiScale;
	const bool scaleChanged = scaleDelta < -0.01f || scaleDelta > 0.01f;
	if (uiScaleInitialized && !scaleChanged)
	{
		return;
	}

	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(targetScale);

	ImGuiIO& imguiIo = ImGui::GetIO();
	imguiIo.FontGlobalScale = targetScale;

	currentUiScale = targetScale;
	uiScaleInitialized = true;
}

void ImGuiLayerModule::applyAnchoredPanelLayout(const AnchoredPanelSlot panelSlot) const
{
	const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	if (mainViewport == nullptr)
	{
		return;
	}

	const float layoutScale = currentUiScale > 0.0f ? currentUiScale : 1.0f;
	const ImVec2 workPosition = mainViewport->WorkPos;
	const ImVec2 workSize = mainViewport->WorkSize;
	const float panelGap = 10.0f * layoutScale;
	const float minimumViewportWidth = 560.0f * layoutScale;
	const float minimumTopHeight = 260.0f * layoutScale;
	float bottomPanelHeight = clampFloat(workSize.y * 0.30f, 260.0f * layoutScale, 380.0f * layoutScale);
	if (workSize.y - bottomPanelHeight - panelGap < minimumTopHeight)
	{
		bottomPanelHeight = std::max(180.0f * layoutScale, workSize.y - minimumTopHeight - panelGap);
	}

	const float topPanelHeight = std::max(140.0f * layoutScale, workSize.y - bottomPanelHeight - panelGap);
	float leftPanelWidth = clampFloat(workSize.x * 0.19f, 320.0f * layoutScale, 430.0f * layoutScale);
	float rightPanelWidth = clampFloat(workSize.x * 0.24f, 380.0f * layoutScale, 560.0f * layoutScale);
	const float availableSideWidth = std::max(0.0f, workSize.x - minimumViewportWidth);
	const float desiredSideWidth = leftPanelWidth + rightPanelWidth;
	if (desiredSideWidth > availableSideWidth && desiredSideWidth > 0.0f)
	{
		const float widthScale = availableSideWidth / desiredSideWidth;
		leftPanelWidth *= widthScale;
		rightPanelWidth *= widthScale;
	}

	ImVec2 panelPosition = workPosition;
	ImVec2 panelSize = workSize;
	switch (panelSlot)
	{
	case AnchoredPanelSlot::left:
		panelSize = ImVec2(leftPanelWidth, topPanelHeight);
		break;
	case AnchoredPanelSlot::right:
		panelPosition.x = workPosition.x + workSize.x - rightPanelWidth;
		panelSize = ImVec2(rightPanelWidth, topPanelHeight);
		break;
	case AnchoredPanelSlot::bottom:
		panelPosition.y = workPosition.y + topPanelHeight + panelGap;
		panelSize = ImVec2(workSize.x, bottomPanelHeight);
		break;
	default:
		assert(false && "[ImGuiLayerModule][Assert] reason=anchored_panel_slot_invalid");
		return;
	}

	ImGui::SetNextWindowPos(panelPosition, ImGuiCond_Always);
	ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);
}

float ImGuiLayerModule::calculateUiScale() const
{
	float dpiScale = 1.0f;
	if (frameworkReference != nullptr)
	{
		const WindowsWindowObject* windowObject = frameworkReference->getWindowObject();
		if (windowObject != nullptr)
		{
			dpiScale = windowObject->getDpiScale();
		}
	}

	const int32 screenWidth = GetSystemMetrics(SM_CXSCREEN);
	const int32 screenHeight = GetSystemMetrics(SM_CYSCREEN);
	float resolutionScale = 1.0f;
	if (screenWidth > 0 && screenHeight > 0)
	{
		const float widthScale = static_cast<float>(screenWidth) / 1920.0f;
		const float heightScale = static_cast<float>(screenHeight) / 1080.0f;
		resolutionScale = widthScale < heightScale ? widthScale : heightScale;
	}

	const float preferredScale = dpiScale > resolutionScale ? dpiScale : resolutionScale;
	return clampFloat(preferredScale, 1.0f, 2.5f);
}

void ImGuiLayerModule::OutlinerPanel::build(ImGuiLayerModule& owner, World* world)
{
	owner.applyAnchoredPanelLayout(ImGuiLayerModule::AnchoredPanelSlot::left);
	if (!ImGui::Begin("Outliner", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
	{
		ImGui::End();
		return;
	}

	if (world == nullptr)
	{
		ImGui::TextUnformatted("No active world.");
		ImGui::End();
		return;
	}

	if (owner.selectedEntityIndex != invalidEntityIndex)
	{
		const Entity* selectedEntity = world->getEntityByIndex(owner.selectedEntityIndex);
		if (selectedEntity == nullptr || isEditorCameraEntity(world, selectedEntity))
		{
			owner.selectedEntityIndex = invalidEntityIndex;
		}
	}

	string worldNameText = world->getName();
	if (worldNameText.empty())
	{
		worldNameText = "(unnamed)";
	}

	const bool deleteShortcutPressed = owner.selectedEntityIndex != invalidEntityIndex
		&& ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
		&& !ImGui::GetIO().WantTextInput
		&& !ImGui::IsAnyItemActive()
		&& !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)
		&& ImGui::IsKeyPressed(ImGuiKey_Delete, false);
	if (deleteShortcutPressed)
	{
		owner.tryDeleteSelectedEntity(world);
	}

	if (ImGui::Button("+AddEntity"))
	{
		const uint32 parentEntityIndex = owner.selectedEntityIndex;
		const Entity* parentEntity = parentEntityIndex != invalidEntityIndex ? world->getEntityByIndex(parentEntityIndex) : nullptr;
		const string parentEntityAssetPath = parentEntity != nullptr ? parentEntity->getAssetPath() : "";
		const uint32 newEntityIndex = world->createPlaceableEntity();
		bool addEntityResult = true;
		if (parentEntity != nullptr)
		{
			addEntityResult = world->addChildEntity(parentEntityIndex, newEntityIndex);
		}

		owner.selectedEntityIndex = newEntityIndex;
		if (!addEntityResult)
		{
			owner.lastEditorActionStatus = "add_entity_failed";
		}
		else if (owner.saveActiveWorldImmediate())
		{
			owner.lastEditorActionStatus = "entity_added_and_saved";
			const Entity* newEntity = world->getEntityByIndex(newEntityIndex);
			if (newEntity != nullptr)
			{
				owner.recordEditorReplayCommand(
					"Editor.addEntity",
					{ newEntity->getAssetPath(), parentEntityAssetPath, PlaceableEntity::getStaticAssetTypeName() });
			}
		}
		else
		{
			owner.lastEditorActionStatus = "entity_added_save_skipped";
		}
	}

	const uint32 entityCount = world->getEntityCount();
	ImGuiTreeNodeFlags worldTreeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (owner.selectedEntityIndex == invalidEntityIndex)
	{
		worldTreeNodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	const bool isWorldNodeOpened = ImGui::TreeNodeEx("World", worldTreeNodeFlags, "%s(World)", worldNameText.c_str());
	if (ImGui::IsItemClicked())
	{
		owner.selectedEntityIndex = invalidEntityIndex;
	}
	if (ImGui::BeginDragDropTarget())
	{
		const ImGuiPayload* dragDropPayload = ImGui::AcceptDragDropPayload(outlinerEntityDragDropPayloadType);
		if (dragDropPayload != nullptr && dragDropPayload->DataSize == sizeof(uint32) && dragDropPayload->IsDelivery())
		{
			const uint32 droppedEntityIndex = *static_cast<const uint32*>(dragDropPayload->Data);
			owner.tryReparentEntity(world, droppedEntityIndex, invalidEntityIndex);
		}

		ImGui::EndDragDropTarget();
	}

	if (isWorldNodeOpened)
	{
		uint32 rootEntityCount = 0;
		for (uint32 entityIndex = 0; entityIndex < entityCount; ++entityIndex)
		{
			const Entity* entity = world->getEntityByIndex(entityIndex);
			if (entity == nullptr || isImmutableEntity(world, entity))
			{
				continue;
			}

			const Entity* parentEntity = world->getEntityByIndex(entity->getParentEntityIndex());
			if (parentEntity != nullptr && !isImmutableEntity(world, parentEntity))
			{
				continue;
			}

			drawEntityNode(owner, world, entityIndex, false);
			++rootEntityCount;
		}

		if (rootEntityCount == 0)
		{
			ImGui::TextUnformatted("No root entities.");
		}

		ImGui::TreePop();
	}

	uint32 immutableRootEntityCount = 0;
	for (uint32 entityIndex = 0; entityIndex < entityCount; ++entityIndex)
	{
		const Entity* entity = world->getEntityByIndex(entityIndex);
		if (entity == nullptr || !isImmutableEntity(world, entity))
		{
			continue;
		}

		const Entity* parentEntity = world->getEntityByIndex(entity->getParentEntityIndex());
		if (parentEntity != nullptr && isImmutableEntity(world, parentEntity))
		{
			continue;
		}

		++immutableRootEntityCount;
	}

	if (immutableRootEntityCount > 0)
	{
		const bool isImmutableTreeOpened = ImGui::TreeNodeEx("Immutable", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick, "%s (Immutable)", worldNameText.c_str());
		if (isImmutableTreeOpened)
		{
			for (uint32 entityIndex = 0; entityIndex < entityCount; ++entityIndex)
			{
				const Entity* entity = world->getEntityByIndex(entityIndex);
				if (entity == nullptr || !isImmutableEntity(world, entity))
				{
					continue;
				}

				const Entity* parentEntity = world->getEntityByIndex(entity->getParentEntityIndex());
				if (parentEntity != nullptr && isImmutableEntity(world, parentEntity))
				{
					continue;
				}

				drawEntityNode(owner, world, entityIndex, true);
			}

			ImGui::TreePop();
		}
	}

	if (!owner.lastEditorActionStatus.empty())
	{
		ImGui::Separator();
		ImGui::Text("Status: %s", owner.lastEditorActionStatus.c_str());
	}

	ImGui::End();
}

void ImGuiLayerModule::OutlinerPanel::drawEntityNode(
	ImGuiLayerModule& owner,
	World* world,
	const uint32 entityIndex,
	const bool immutableTree)
{
	if (world == nullptr)
	{
		return;
	}

	const Entity* entity = world->getEntityByIndex(entityIndex);
	if (entity == nullptr)
	{
		return;
	}

	const bool immutableEntity = isImmutableEntity(world, entity);
	if (immutableEntity != immutableTree)
	{
		return;
	}

	ImGui::PushID(static_cast<int32>(entityIndex));
	const string entityDisplayText = buildEntityDisplayText(world, entity);

	ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (owner.selectedEntityIndex == entityIndex && !immutableEntity)
	{
		treeNodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	bool hasVisibleChildEntity = false;
	uint32 childEntityIndex = entity->getFirstChildEntityIndex();
	uint32 childGuardCount = 0;
	const uint32 maxChildGuardCount = world->getEntityCount();
	while (childEntityIndex != invalidEntityIndex && childGuardCount < maxChildGuardCount)
	{
		const Entity* childEntity = world->getEntityByIndex(childEntityIndex);
		if (childEntity == nullptr)
		{
			break;
		}

		if (isImmutableEntity(world, childEntity) == immutableTree)
		{
			hasVisibleChildEntity = true;
			break;
		}

		childEntityIndex = childEntity->getNextSiblingEntityIndex();
		++childGuardCount;
	}

	if (!hasVisibleChildEntity)
	{
		treeNodeFlags |= ImGuiTreeNodeFlags_Leaf;
	}

	const bool isNodeOpened = ImGui::TreeNodeEx(
		"Entity",
		treeNodeFlags,
		"%s",
		entityDisplayText.c_str());
	if (ImGui::IsItemClicked() && !immutableEntity)
	{
		owner.selectedEntityIndex = entityIndex;
	}
	if (!immutableEntity && ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload(outlinerEntityDragDropPayloadType, &entityIndex, sizeof(entityIndex));
		ImGui::Text("%s", entityDisplayText.c_str());
		ImGui::EndDragDropSource();
	}
	if (!immutableEntity && ImGui::BeginDragDropTarget())
	{
		const ImGuiPayload* dragDropPayload = ImGui::AcceptDragDropPayload(outlinerEntityDragDropPayloadType);
		if (dragDropPayload != nullptr && dragDropPayload->DataSize == sizeof(uint32) && dragDropPayload->IsDelivery())
		{
			const uint32 droppedEntityIndex = *static_cast<const uint32*>(dragDropPayload->Data);
			owner.tryReparentEntity(world, droppedEntityIndex, entityIndex);
		}

		ImGui::EndDragDropTarget();
	}

	if (isNodeOpened)
	{
		childEntityIndex = entity->getFirstChildEntityIndex();
		childGuardCount = 0;
		while (childEntityIndex != invalidEntityIndex && childGuardCount < maxChildGuardCount)
		{
			const Entity* childEntity = world->getEntityByIndex(childEntityIndex);
			if (childEntity == nullptr)
			{
				break;
			}

			const uint32 nextChildEntityIndex = childEntity->getNextSiblingEntityIndex();
			if (isImmutableEntity(world, childEntity) == immutableTree)
			{
				drawEntityNode(owner, world, childEntityIndex, immutableTree);
			}

			childEntityIndex = nextChildEntityIndex;
			++childGuardCount;
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

void ImGuiLayerModule::DetailPanel::reset()
{
	meshAssetRootPath.clear();
	materialAssetRootPath.clear();
	meshAssetPaths.clear();
	materialAssetPaths.clear();
	materialAssetDirtyStateByPath.clear();
	createMaterialAssetNameText = "NewMaterial";
	createMaterialAssetStatusText.clear();
	meshAssetPathsLoaded = false;
	materialAssetPathsLoaded = false;
}

bool ImGuiLayerModule::DetailPanel::ensureMeshAssetPathsLoaded()
{
	if (meshAssetPathsLoaded)
	{
		return !meshAssetRootPath.empty();
	}

	meshAssetRootPath = DiskLoaderModule::get()->resolveAbsolutePathFromResources("Meshes");
	meshAssetPaths.clear();
	collectMeshAssetPaths(meshAssetRootPath);
	meshAssetPathsLoaded = true;
	return true;
}

void ImGuiLayerModule::DetailPanel::collectMeshAssetPaths(const filesystem_path& directoryPath)
{
	error_code directoryErrorCode;
	vector<filesystem_directory_entry> directoryEntries = {};
	for (const filesystem_directory_entry& directoryEntry : filesystem_directory_iterator(directoryPath, filesystem_directory_options::skip_permission_denied, directoryErrorCode))
	{
		if (directoryErrorCode)
		{
			break;
		}

		directoryEntries.push_back(directoryEntry);
	}

	sortDirectoryEntries(directoryEntries);
	for (uint32 directoryEntryIndex = 0; directoryEntryIndex < static_cast<uint32>(directoryEntries.size()); ++directoryEntryIndex)
	{
		const filesystem_directory_entry& directoryEntry = directoryEntries[directoryEntryIndex];
		if (isDirectoryEntry(directoryEntry))
		{
			collectMeshAssetPaths(directoryEntry.path());
			continue;
		}

		string extension = directoryEntry.path().extension().string();
		tolower(extension);
		if (extension != ".deasset")
		{
			continue;
		}

		const filesystem_path relativePath = directoryEntry.path().lexically_normal().lexically_relative(meshAssetRootPath);
		meshAssetPaths.push_back((filesystem_path("Meshes") / relativePath).lexically_normal().string());
	}
}

bool ImGuiLayerModule::DetailPanel::ensureMaterialAssetPathsLoaded()
{
	if (materialAssetPathsLoaded)
	{
		return !materialAssetRootPath.empty();
	}

	materialAssetRootPath = DiskLoaderModule::get()->resolveAbsolutePathFromResources("Materials");
	materialAssetPaths.clear();
	collectMaterialAssetPaths(materialAssetRootPath);
	materialAssetPathsLoaded = true;
	return true;
}

void ImGuiLayerModule::DetailPanel::collectMaterialAssetPaths(const filesystem_path& directoryPath)
{
	error_code directoryErrorCode;
	vector<filesystem_directory_entry> directoryEntries = {};
	for (const filesystem_directory_entry& directoryEntry : filesystem_directory_iterator(directoryPath, filesystem_directory_options::skip_permission_denied, directoryErrorCode))
	{
		if (directoryErrorCode)
		{
			break;
		}

		directoryEntries.push_back(directoryEntry);
	}

	sortDirectoryEntries(directoryEntries);
	for (uint32 directoryEntryIndex = 0; directoryEntryIndex < static_cast<uint32>(directoryEntries.size()); ++directoryEntryIndex)
	{
		const filesystem_directory_entry& directoryEntry = directoryEntries[directoryEntryIndex];
		if (isDirectoryEntry(directoryEntry))
		{
			collectMaterialAssetPaths(directoryEntry.path());
			continue;
		}

		string extension = directoryEntry.path().extension().string();
		tolower(extension);
		if (extension != ".deasset")
		{
			continue;
		}

		const filesystem_path relativePath = directoryEntry.path().lexically_normal().lexically_relative(materialAssetRootPath);
		materialAssetPaths.push_back((filesystem_path("Materials") / relativePath).lexically_normal().string());
	}
}

void ImGuiLayerModule::DetailPanel::build(ImGuiLayerModule& owner, World* world)
{
	owner.applyAnchoredPanelLayout(ImGuiLayerModule::AnchoredPanelSlot::right);
	if (!ImGui::Begin("Detail", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
	{
		ImGui::End();
		return;
	}

	if (world == nullptr)
	{
		ImGui::TextUnformatted("No active world.");
		ImGui::End();
		return;
	}

	if (owner.selectedEntityIndex == invalidEntityIndex)
	{
		ImGui::TextUnformatted("Select an entity from Outliner.");
		ImGui::End();
		return;
	}

	Entity* selectedEntity = world->getEntityByIndex(owner.selectedEntityIndex);
	if (selectedEntity == nullptr)
	{
		owner.selectedEntityIndex = invalidEntityIndex;
		ImGui::TextUnformatted("Selected entity is invalid.");
		ImGui::End();
		return;
	}
	if (isEditorCameraEntity(world, selectedEntity))
	{
		owner.selectedEntityIndex = invalidEntityIndex;
		ImGui::TextUnformatted("Select an entity from Outliner.");
		ImGui::End();
		return;
	}

	const string selectedEntityDisplayText = buildEntityDisplayText(world, selectedEntity);
	ImGui::Text("%s", selectedEntityDisplayText.c_str());
	string entityName = selectedEntity->getName();
	if (ImGui::InputText("Name", &entityName))
	{
		selectedEntity->setName(entityName);
		if (owner.saveActiveWorldImmediate())
		{
			owner.lastEditorActionStatus = "entity_name_updated_and_saved";
			owner.recordEditorReplayCommand("Editor.setEntityName", { selectedEntity->getAssetPath(), entityName });
		}
		else
		{
			owner.lastEditorActionStatus = "entity_name_updated_save_skipped";
		}
	}
	ImGui::Text("Active: %s", selectedEntity->isActive() ? "true" : "false");
	if (selectedEntity->getParentEntityIndex() == invalidEntityIndex)
	{
		ImGui::TextUnformatted("Parent: invalid");
	}
	else
	{
		ImGui::Text("Parent: %u", selectedEntity->getParentEntityIndex());
	}
	if (selectedEntity->getFirstChildEntityIndex() == invalidEntityIndex)
	{
		ImGui::TextUnformatted("First Child: invalid");
	}
	else
	{
		ImGui::Text("First Child: %u", selectedEntity->getFirstChildEntityIndex());
	}
	if (selectedEntity->getNextSiblingEntityIndex() == invalidEntityIndex)
	{
		ImGui::TextUnformatted("Next Sibling: invalid");
	}
	else
	{
		ImGui::Text("Next Sibling: %u", selectedEntity->getNextSiblingEntityIndex());
	}

	ImGui::Separator();
	const uint32 componentCount = selectedEntity->getComponentCount();
	ImGui::Text("Components: %u", componentCount);

	PlaceableEntity* placeableEntity = dynamic_cast<PlaceableEntity*>(selectedEntity);
	MeshComponent* meshComponent = nullptr;
	CameraComponent* cameraComponent = nullptr;
	for (uint32 componentArrayIndex = 0; componentArrayIndex < componentCount; ++componentArrayIndex)
	{
		const uint32 componentIndex = selectedEntity->getComponentIndex(componentArrayIndex);
		ImGui::BulletText("Component Index: %u", componentIndex);

		Component* component = world->getComponentByIndex(componentIndex);
		if (component == nullptr)
		{
			continue;
		}

		if (component->getComponentType() == MeshComponent::staticComponentType && meshComponent == nullptr)
		{
			meshComponent = static_cast<MeshComponent*>(component);
		}
		else if (component->getComponentType() == CameraComponent::staticComponentType && cameraComponent == nullptr)
		{
			cameraComponent = static_cast<CameraComponent*>(component);
		}
	}

	const bool deleteShortcutPressed = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
		&& !ImGui::GetIO().WantTextInput
		&& !ImGui::IsAnyItemActive()
		&& !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)
		&& ImGui::IsKeyPressed(ImGuiKey_Delete, false);
	const bool canDeleteSelectedEntity = cameraComponent == nullptr || !cameraComponent->editorCamera;
	ImGui::BeginDisabled(!canDeleteSelectedEntity);
	const bool deleteButtonPressed = ImGui::Button("Delete Entity");
	ImGui::EndDisabled();
	if (!canDeleteSelectedEntity)
	{
		ImGui::TextDisabled("Editor camera entity cannot be deleted.");
		if (deleteShortcutPressed)
		{
			owner.lastEditorActionStatus = "editor_camera_entity_delete_blocked";
		}
	}

	if ((deleteButtonPressed || (canDeleteSelectedEntity && deleteShortcutPressed))
		&& owner.tryDeleteSelectedEntity(world))
	{
		ImGui::End();
		return;
	}

	ImGui::Separator();
	const bool hasMeshComponent = meshComponent != nullptr;
	const bool hasCameraComponent = cameraComponent != nullptr;
	if (ImGui::Button("Add Component"))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}
	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		ImGui::BeginDisabled(hasMeshComponent);
		if (ImGui::MenuItem("Mesh"))
		{
			unique_pointer<MeshComponent> newMeshComponent(new MeshComponent());
			if (selectedEntity->addComponent(moveValue(newMeshComponent)))
			{
				if (owner.saveActiveWorldImmediate())
				{
					owner.lastEditorActionStatus = "mesh_component_added_and_saved";
					MeshComponent* savedMeshComponent = componentCast<MeshComponent>(
						findFirstComponentByType(world, selectedEntity, MeshComponent::staticComponentType));
					if (savedMeshComponent != nullptr)
					{
						owner.recordEditorReplayCommand(
							"Editor.addComponent",
							{ selectedEntity->getAssetPath(), MeshComponent::getStaticAssetTypeName(), savedMeshComponent->getAssetPath() });
					}
				}
				else
				{
					owner.lastEditorActionStatus = "mesh_component_added_save_skipped";
				}
			}
			else
			{
				owner.lastEditorActionStatus = "mesh_component_add_failed";
			}
		}
		ImGui::EndDisabled();

		ImGui::BeginDisabled(placeableEntity == nullptr || hasCameraComponent);
		if (ImGui::MenuItem("Camera"))
		{
			unique_pointer<CameraComponent> newCameraComponent(new CameraComponent());
			if (selectedEntity->addComponent(moveValue(newCameraComponent)))
			{
				if (owner.saveActiveWorldImmediate())
				{
					owner.lastEditorActionStatus = "camera_component_added_and_saved";
					CameraComponent* savedCameraComponent = componentCast<CameraComponent>(
						findFirstComponentByType(world, selectedEntity, CameraComponent::staticComponentType));
					if (savedCameraComponent != nullptr)
					{
						owner.recordEditorReplayCommand(
							"Editor.addComponent",
							{ selectedEntity->getAssetPath(), CameraComponent::getStaticAssetTypeName(), savedCameraComponent->getAssetPath() });
					}
				}
				else
				{
					owner.lastEditorActionStatus = "camera_component_added_save_skipped";
				}
			}
			else
			{
				owner.lastEditorActionStatus = "camera_component_add_failed";
			}
		}
		ImGui::EndDisabled();

		if (placeableEntity == nullptr)
		{
			ImGui::Separator();
			ImGui::TextDisabled("Camera requires PlaceableEntity.");
		}

		ImGui::EndPopup();
	}

	if (hasMeshComponent)
	{
		ImGui::Separator();
		ImGui::TextUnformatted("MeshComponent");

		int32 lodLevel = static_cast<int32>(meshComponent->getLODLevel());
		bool visible = meshComponent->isVisible();
		bool meshComponentChanged = false;
		bool meshStreamingRefreshRequired = false;
		const string& meshAssetPath = meshComponent->getMeshAssetPath();
		const char* meshAssetPathText = meshAssetPath.empty() ? "(none)" : meshAssetPath.c_str();

		ImGui::Text("Mesh Asset: %s", meshAssetPathText);
		if (ImGui::Button("Select Mesh Asset"))
		{
			meshAssetPathsLoaded = false;
			ImGui::OpenPopup("Select Mesh Asset");
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Mesh Asset"))
		{
			meshComponent->setMeshAssetPath("");
			meshComponentChanged = true;
		}

		if (ImGui::BeginPopupModal("Select Mesh Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (!ensureMeshAssetPathsLoaded())
			{
				ImGui::TextDisabled("Meshes directory not found.");
			}
			else if (meshAssetPaths.empty())
			{
				ImGui::TextDisabled("No mesh assets found.");
			}
			else
			{
				for (uint32 meshAssetPathIndex = 0; meshAssetPathIndex < static_cast<uint32>(meshAssetPaths.size()); ++meshAssetPathIndex)
				{
					const string& meshAssetPath = meshAssetPaths[meshAssetPathIndex];
					if (!ImGui::Selectable(meshAssetPath.c_str(), meshComponent->getMeshAssetPath() == meshAssetPath))
					{
						continue;
					}

					meshComponent->setMeshAssetPath(meshAssetPath);
					meshComponentChanged = true;
					meshStreamingRefreshRequired = true;
					ImGui::CloseCurrentPopup();
					break;
				}
			}

			if (ImGui::Button("Close"))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		if (ImGui::InputInt("LOD", &lodLevel))
		{
			if (lodLevel < 0)
			{
				lodLevel = 0;
			}

			meshComponent->setLODLevel(static_cast<uint32>(lodLevel));
			meshComponentChanged = true;
			meshStreamingRefreshRequired = meshComponent->hasLoadedMeshAsset();
		}

		if (ImGui::Checkbox("Visible", &visible))
		{
			meshComponent->setVisible(visible);
			meshComponentChanged = true;
		}

		uint32 sectionCount = static_cast<uint32>(meshComponent->getMaterialAssetPaths().size());
		sectionCount = std::max(sectionCount, meshComponent->getMeshSectionCount());

		ImGui::Separator();
		ImGui::Text("Material Sections: %u", sectionCount);
		if (!meshComponent->hasLoadedMeshAsset())
		{
			ImGui::TextDisabled("Assign a mesh asset to edit per-section materials.");
		}
		else if (sectionCount == 0)
		{
			ImGui::TextDisabled("Mesh asset has no section ranges.");
		}

		for (uint32 sectionIndex = 0; sectionIndex < sectionCount; ++sectionIndex)
		{
			const vector<string>& currentMaterialAssetPaths = meshComponent->getMaterialAssetPaths();
			const string currentMaterialAssetPath =
				sectionIndex < static_cast<uint32>(currentMaterialAssetPaths.size())
					? currentMaterialAssetPaths[sectionIndex]
					: "";
			shared_pointer<MaterialAsset> materialAsset = meshComponent->getMaterialAsset(sectionIndex);

			ImGui::PushID(static_cast<int32>(sectionIndex));
			if (ImGui::TreeNodeEx("SectionMaterial", ImGuiTreeNodeFlags_DefaultOpen, "Section %u", sectionIndex))
			{
				ImGui::Text("Material Asset: %s", currentMaterialAssetPath.empty() ? "(none)" : currentMaterialAssetPath.c_str());
				if (ImGui::Button("Select Material Asset"))
				{
					materialAssetPathsLoaded = false;
					ImGui::OpenPopup("Select Material Asset");
				}
				ImGui::SameLine();
				ImGui::BeginDisabled(currentMaterialAssetPath.empty());
				if (ImGui::Button("Clear Material Asset"))
				{
					meshComponent->setMaterialAssetPath(sectionIndex, "");
					meshComponentChanged = true;
				}
				ImGui::EndDisabled();
				ImGui::SameLine();
				if (ImGui::Button(currentMaterialAssetPath.empty() ? "Create Material Asset" : "Duplicate Material Asset"))
				{
					createMaterialAssetNameText =
						materialAsset != nullptr && !materialAsset->getName().empty()
							? (materialAsset->getName() + "_Copy")
							: "NewMaterial";
					createMaterialAssetStatusText.clear();
					ImGui::OpenPopup("Create Material Asset");
				}

				if (ImGui::BeginPopupModal("Select Material Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
				{
					if (!ensureMaterialAssetPathsLoaded())
					{
						ImGui::TextDisabled("Materials directory not found.");
					}
					else if (materialAssetPaths.empty())
					{
						ImGui::TextDisabled("No material assets found.");
					}
					else
					{
						for (uint32 materialAssetPathIndex = 0; materialAssetPathIndex < static_cast<uint32>(materialAssetPaths.size()); ++materialAssetPathIndex)
						{
							const string& materialAssetPath = materialAssetPaths[materialAssetPathIndex];
							if (!ImGui::Selectable(materialAssetPath.c_str(), currentMaterialAssetPath == materialAssetPath))
							{
								continue;
							}

							meshComponent->setMaterialAssetPath(sectionIndex, materialAssetPath);
							meshComponentChanged = true;
							ImGui::CloseCurrentPopup();
							break;
						}
					}

					if (ImGui::Button("Close"))
					{
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}

				if (ImGui::BeginPopupModal("Create Material Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::Text(
						"Source: %s",
						materialAsset != nullptr && !currentMaterialAssetPath.empty()
							? currentMaterialAssetPath.c_str()
							: "(default)");
					ImGui::InputText("Material Name", &createMaterialAssetNameText);
					if (!createMaterialAssetStatusText.empty())
					{
						ImGui::TextWrapped("Status: %s", createMaterialAssetStatusText.c_str());
					}

					if (ImGui::Button("Create"))
					{
						createMaterialAssetStatusText.clear();
						if (!ensureMaterialAssetPathsLoaded())
						{
							createMaterialAssetStatusText = "material_root_missing";
						}
						else
						{
							shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
							const string materialFileStem = diskLoaderModule->sanitizeFileName(createMaterialAssetNameText, "NewMaterial");
							string createdAbsoluteMaterialAssetPath = {};
							if (!diskLoaderModule->resolveUniqueFilePath(materialAssetRootPath, materialFileStem, ".deasset", createdAbsoluteMaterialAssetPath))
							{
								createMaterialAssetStatusText = "material_path_create_failed";
							}
							else
							{
								const filesystem_path relativeMaterialAssetPath =
									filesystem_path(createdAbsoluteMaterialAssetPath).lexically_normal().lexically_relative(materialAssetRootPath);
								const string createdMaterialAssetPath =
									(filesystem_path("Materials") / relativeMaterialAssetPath).lexically_normal().string();
								const string createdMaterialAssetName = filesystem_path(createdMaterialAssetPath).stem().string();
								const string shaderTemplatePath =
									materialAsset != nullptr ? materialAsset->getShaderTemplatePath() : MaterialAsset::getDefaultShaderTemplatePath();
								const string shaderPackagePath =
									materialAsset != nullptr ? materialAsset->getShaderPackagePath() : MaterialAsset::getDefaultShaderPackagePath();
								const string shaderVariantName =
									materialAsset != nullptr ? materialAsset->getShaderVariantName() : MaterialAsset::getDefaultShaderVariantName();
								const string vertexShaderInjectedCode =
									materialAsset != nullptr ? materialAsset->getVertexShaderInjectedCode() : "";
								const string pixelShaderInjectedCode =
									materialAsset != nullptr ? materialAsset->getPixelShaderInjectedCode() : "";
								const string createCommandText = buildReplayCreateMaterialAssetCommandText(
									createdMaterialAssetPath,
									createdMaterialAssetName,
									shaderTemplatePath,
									shaderPackagePath,
									shaderVariantName,
									vertexShaderInjectedCode,
									pixelShaderInjectedCode);
								const bool createdMaterialAsset =
									CLIModule::execute(createCommandText)
									&& CLIModule::get() != nullptr
									&& CLIModule::get()->getLastExecutionCode() == static_cast<int32>(CLIModule::ExecutionCode::succeeded);
								if (!createdMaterialAsset)
								{
									createMaterialAssetStatusText = "material_create_failed";
								}
								else
								{
									materialAssetPathsLoaded = false;
									materialAssetDirtyStateByPath[createdMaterialAssetPath] = false;
									meshComponent->setMaterialAssetPath(sectionIndex, createdMaterialAssetPath);
									meshComponentChanged = true;
									owner.lastEditorActionStatus = "material_asset_created";
									owner.recordEditorReplayCommandText(createCommandText);
									ImGui::CloseCurrentPopup();
								}
							}
						}
					}

					ImGui::SameLine();
					if (ImGui::Button("Cancel"))
					{
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}

				materialAsset = meshComponent->getMaterialAsset(sectionIndex);
				const vector<string>& updatedMaterialAssetPaths = meshComponent->getMaterialAssetPaths();
				const string updatedMaterialAssetPath =
					sectionIndex < static_cast<uint32>(updatedMaterialAssetPaths.size())
						? updatedMaterialAssetPaths[sectionIndex]
						: "";
				if (materialAsset == nullptr)
				{
					if (!updatedMaterialAssetPath.empty())
					{
						ImGui::TextDisabled("Material asset load failed.");
					}
				}
				else
				{
					string shaderTemplatePath = materialAsset->getShaderTemplatePath();
					string shaderPackagePath = materialAsset->getShaderPackagePath();
					string shaderVariantName = materialAsset->getShaderVariantName();
					string vertexShaderInjectedCode = materialAsset->getVertexShaderInjectedCode();
					string pixelShaderInjectedCode = materialAsset->getPixelShaderInjectedCode();
					bool materialAssetChanged = false;

					if (ImGui::InputText("Shader Template", &shaderTemplatePath))
					{
						materialAssetChanged = true;
					}

					if (ImGui::InputText("Shader Package", &shaderPackagePath))
					{
						materialAssetChanged = true;
					}

					if (ImGui::InputText("Shader Variant", &shaderVariantName))
					{
						materialAssetChanged = true;
					}

					if (ImGui::InputTextMultiline("Vertex Injection", &vertexShaderInjectedCode, ImVec2(-1.0f, 110.0f)))
					{
						materialAssetChanged = true;
					}

					if (ImGui::InputTextMultiline("Pixel Injection", &pixelShaderInjectedCode, ImVec2(-1.0f, 140.0f)))
					{
						materialAssetChanged = true;
					}

					if (materialAssetChanged)
					{
						editorCommandSyncLoadedMaterialShaderConfig(
							world,
							materialAsset->getAssetPath(),
							shaderTemplatePath,
							shaderPackagePath,
							shaderVariantName,
							vertexShaderInjectedCode,
							pixelShaderInjectedCode);
						materialAssetDirtyStateByPath[materialAsset->getAssetPath()] = true;
						owner.lastEditorActionStatus = "material_asset_modified";
					}

					bool materialAssetDirty = false;
					const auto dirtyStateIterator = materialAssetDirtyStateByPath.find(materialAsset->getAssetPath());
					if (dirtyStateIterator != materialAssetDirtyStateByPath.end())
					{
						materialAssetDirty = dirtyStateIterator->second;
					}

					ImGui::BeginDisabled(!materialAssetDirty || materialAsset->getAssetPath().empty());
					if (ImGui::Button("Save Material"))
					{
						const string saveCommandText = buildReplaySetMaterialShaderCommandText(*materialAsset);
						const bool savedMaterialAsset =
							CLIModule::execute(saveCommandText)
							&& CLIModule::get() != nullptr
							&& CLIModule::get()->getLastExecutionCode() == static_cast<int32>(CLIModule::ExecutionCode::succeeded);
						if (savedMaterialAsset)
						{
							materialAssetDirtyStateByPath[materialAsset->getAssetPath()] = false;
							owner.lastEditorActionStatus = "material_asset_saved";
							owner.recordEditorReplayCommandText(saveCommandText);
						}
						else
						{
							owner.lastEditorActionStatus = "material_asset_save_failed";
						}
					}
					ImGui::EndDisabled();

					string generatedShaderSourceText = {};
					const bool generatedShaderSourceAvailable = materialAsset->buildShaderSourceText(generatedShaderSourceText);
					if (ImGui::TreeNodeEx("GeneratedShader", ImGuiTreeNodeFlags_DefaultOpen, "Generated Shader"))
					{
						if (generatedShaderSourceAvailable)
						{
							ImGui::InputTextMultiline(
								"##GeneratedShaderSource",
								&generatedShaderSourceText,
								ImVec2(-1.0f, 180.0f),
								ImGuiInputTextFlags_ReadOnly);
						}
						else
						{
							ImGui::TextDisabled("Failed to build material shader source.");
						}

						ImGui::TreePop();
					}
				}

				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		if (meshComponentChanged)
		{
			if (meshStreamingRefreshRequired && meshComponent->hasLoadedMeshAsset())
			{
				meshComponent->requestMeshStreaming();
			}

			if (owner.saveActiveWorldImmediate())
			{
				owner.lastEditorActionStatus = "mesh_component_updated_and_saved";
				owner.recordEditorReplayCommand("Editor.setMeshComponent", buildReplayMeshComponentArguments(*meshComponent));
			}
			else
			{
				owner.lastEditorActionStatus = "mesh_component_updated_save_skipped";
			}
		}
	}

	if (hasCameraComponent)
	{
		ImGui::Separator();
		ImGui::TextUnformatted("CameraComponent");

		bool primary = cameraComponent->primary;
		float fieldOfViewYDegrees = cameraComponent->fieldOfViewYDegrees;
		float nearPlane = cameraComponent->nearPlane;
		float farPlane = cameraComponent->farPlane;
		bool cameraComponentChanged = false;

		if (ImGui::Checkbox("Primary", &primary))
		{
			cameraComponentChanged = true;
		}

		if (ImGui::InputFloat("Field Of View Y", &fieldOfViewYDegrees, 1.0f, 10.0f, "%.3f"))
		{
			cameraComponentChanged = true;
		}

		if (ImGui::InputFloat("Near Plane", &nearPlane, 0.01f, 0.1f, "%.3f"))
		{
			cameraComponentChanged = true;
		}

		if (ImGui::InputFloat("Far Plane", &farPlane, 1.0f, 10.0f, "%.3f"))
		{
			cameraComponentChanged = true;
		}

		ImGui::Text("Editor Camera: %s", cameraComponent->editorCamera ? "true" : "false");

		if (cameraComponentChanged)
		{
			fieldOfViewYDegrees = clampCameraFieldOfViewYDegrees(fieldOfViewYDegrees);
			clampCameraPlanes(nearPlane, farPlane);
			cameraComponent->primary = primary;
			cameraComponent->fieldOfViewYDegrees = fieldOfViewYDegrees;
			cameraComponent->nearPlane = nearPlane;
			cameraComponent->farPlane = farPlane;

			if (owner.saveActiveWorldImmediate())
			{
				owner.lastEditorActionStatus = "camera_component_updated_and_saved";
				owner.recordEditorReplayCommand(
					"Editor.setCameraComponent",
					{
						cameraComponent->getAssetPath(),
						buildReplayBooleanArgument(cameraComponent->primary),
						buildReplayFloatArgument(cameraComponent->fieldOfViewYDegrees),
						buildReplayFloatArgument(cameraComponent->nearPlane),
						buildReplayFloatArgument(cameraComponent->farPlane)
					});
			}
			else
			{
				owner.lastEditorActionStatus = "camera_component_updated_save_skipped";
			}
		}
	}

	if (placeableEntity != nullptr)
	{
		Transform updatedTransform = placeableEntity->transform;
		bool transformChanged = false;
		float position[3] = {
			updatedTransform.positionX,
			updatedTransform.positionY,
			updatedTransform.positionZ};
		float rotation[3] = {
			updatedTransform.rotationPitch,
			updatedTransform.rotationYaw,
			updatedTransform.rotationRoll};
		float scale[3] = {
			updatedTransform.scaleX,
			updatedTransform.scaleY,
			updatedTransform.scaleZ};
		ImGui::Separator();
		ImGui::TextUnformatted("Transform");

		if (ImGui::InputFloat3("Position", position, "%.3f"))
		{
			updatedTransform.positionX = position[0];
			updatedTransform.positionY = position[1];
			updatedTransform.positionZ = position[2];
			transformChanged = true;
		}

		if (ImGui::InputFloat3("Rotation", rotation, "%.3f"))
		{
			updatedTransform.rotationPitch = rotation[0];
			updatedTransform.rotationYaw = rotation[1];
			updatedTransform.rotationRoll = rotation[2];
			transformChanged = true;
		}

		if (ImGui::InputFloat3("Scale", scale, "%.3f"))
		{
			updatedTransform.scaleX = scale[0];
			updatedTransform.scaleY = scale[1];
			updatedTransform.scaleZ = scale[2];
			transformChanged = true;
		}

		if (transformChanged)
		{
			placeableEntity->transform = updatedTransform;
			if (owner.saveActiveWorldImmediate())
			{
				owner.lastEditorActionStatus = "entity_transform_updated_and_saved";
				owner.recordEditorReplayCommand(
					"Editor.setTransform",
					{
						placeableEntity->getAssetPath(),
						buildReplayFloatArgument(placeableEntity->transform.positionX),
						buildReplayFloatArgument(placeableEntity->transform.positionY),
						buildReplayFloatArgument(placeableEntity->transform.positionZ),
						buildReplayFloatArgument(placeableEntity->transform.rotationPitch),
						buildReplayFloatArgument(placeableEntity->transform.rotationYaw),
						buildReplayFloatArgument(placeableEntity->transform.rotationRoll),
						buildReplayFloatArgument(placeableEntity->transform.scaleX),
						buildReplayFloatArgument(placeableEntity->transform.scaleY),
						buildReplayFloatArgument(placeableEntity->transform.scaleZ)
					});
			}
			else
			{
				owner.lastEditorActionStatus = "entity_transform_updated_save_skipped";
			}
		}
	}

	ImGui::End();
}

bool ImGuiLayerModule::tryDeleteSelectedEntity(World* world)
{
	if (world == nullptr || selectedEntityIndex == invalidEntityIndex)
	{
		return false;
	}

	const Entity* selectedEntity = world->getEntityByIndex(selectedEntityIndex);
	if (selectedEntity == nullptr)
	{
		selectedEntityIndex = invalidEntityIndex;
		lastEditorActionStatus = "entity_delete_invalid_selection";
		return false;
	}

	const CameraComponent* cameraComponent = getFirstCameraComponent(world, selectedEntity);
	if (cameraComponent != nullptr && cameraComponent->editorCamera)
	{
		lastEditorActionStatus = "editor_camera_entity_delete_blocked";
		return false;
	}

	const uint32 entityIndexToDelete = selectedEntityIndex;
	const string deletedEntityAssetPath = selectedEntity->getAssetPath();
	if (!world->removeEntity(entityIndexToDelete))
	{
		lastEditorActionStatus = "entity_delete_failed";
		return false;
	}

	selectedEntityIndex = invalidEntityIndex;
	if (saveActiveWorldImmediate())
	{
		lastEditorActionStatus = "entity_deleted_and_saved";
		recordEditorReplayCommand("Editor.deleteEntity", { deletedEntityAssetPath });
	}
	else
	{
		lastEditorActionStatus = "entity_deleted_save_skipped";
	}
	return true;
}

bool ImGuiLayerModule::tryReparentEntity(World* world, const uint32 childEntityIndex, const uint32 parentEntityIndex)
{
	if (world == nullptr)
	{
		return false;
	}

	if (childEntityIndex == parentEntityIndex)
	{
		lastEditorActionStatus = "entity_reparent_self_blocked";
		return false;
	}

	const Entity* childEntity = world->getEntityByIndex(childEntityIndex);
	if (childEntity == nullptr)
	{
		lastEditorActionStatus = "entity_reparent_invalid_selection";
		return false;
	}

	if (isEditorCameraEntity(world, childEntity))
	{
		lastEditorActionStatus = "editor_camera_entity_reparent_blocked";
		return false;
	}

	if (parentEntityIndex != invalidEntityIndex)
	{
		const Entity* parentEntity = world->getEntityByIndex(parentEntityIndex);
		if (parentEntity == nullptr)
		{
			lastEditorActionStatus = "entity_reparent_invalid_parent";
			return false;
		}

		if (isEditorCameraEntity(world, parentEntity))
		{
			lastEditorActionStatus = "editor_camera_entity_parent_blocked";
			return false;
		}
	}

	selectedEntityIndex = childEntityIndex;
	if (childEntity->getParentEntityIndex() == parentEntityIndex)
	{
		lastEditorActionStatus = "entity_parent_unchanged";
		return true;
	}

	const string childEntityAssetPath = childEntity->getAssetPath();
	const string parentEntityAssetPath = parentEntityIndex != invalidEntityIndex
		? world->getEntityByIndex(parentEntityIndex)->getAssetPath()
		: "";

	if (!world->reparentEntity(childEntityIndex, parentEntityIndex))
	{
		lastEditorActionStatus = "entity_reparent_failed";
		return false;
	}

	if (saveActiveWorldImmediate())
	{
		lastEditorActionStatus = "entity_reparented_and_saved";
		recordEditorReplayCommand("Editor.reparentEntity", { childEntityAssetPath, parentEntityAssetPath });
	}
	else
	{
		lastEditorActionStatus = "entity_reparented_save_skipped";
	}
	return true;
}

void ImGuiLayerModule::DeassetViewerPanel::reset()
{
	opened = false;
	dirty = false;
	absoluteFilePath.clear();
	displayPathText.clear();
	document.clear();
	loadedDocument.clear();
	documentKeys.clear();
	statusText.clear();
}

void ImGuiLayerModule::DeassetViewerPanel::open(const filesystem_path& filePath, const string& displayPath)
{
	reset();

	opened = true;
	absoluteFilePath = filePath.lexically_normal().string();
	displayPathText = displayPath.empty() ? absoluteFilePath : displayPath;
	reloadDocument();
}

const char* ImGuiLayerModule::DeassetViewerPanel::buildParseCodeText(const XML::ParseCode parseCode)
{
	switch (parseCode)
	{
	case XML::ParseCode::succeeded:
		return "succeeded";
	case XML::ParseCode::fileOpenFailed:
		return "file_open_failed";
	case XML::ParseCode::malformedDocument:
		return "malformed_document";
	case XML::ParseCode::duplicateKey:
		return "duplicate_key";
	default:
		return "unknown";
	}
}

bool ImGuiLayerModule::DeassetViewerPanel::reloadDocument()
{
	document.clear();
	documentKeys.clear();
	dirty = false;
	statusText.clear();
	if (absoluteFilePath.empty())
	{
		statusText = "reload_failed: file_path_missing";
		return false;
	}

	const XML::ParseCode parseCode = XML::get().readDocumentFile(absoluteFilePath, document);
	if (parseCode != XML::ParseCode::succeeded)
	{
		statusText = string("reload_failed: ") + buildParseCodeText(parseCode);
		return false;
	}

	loadedDocument = document;
	rebuildDocumentKeys();
	statusText = "loaded";
	return true;
}

bool ImGuiLayerModule::DeassetViewerPanel::saveDocument()
{
	if (absoluteFilePath.empty())
	{
		statusText = "save_failed: file_path_missing";
		return false;
	}

	if (!XML::get().writeDocumentFile(absoluteFilePath, document))
	{
		statusText = "save_failed";
		return false;
	}

	dirty = false;
	statusText = "saved";
	return true;
}

void ImGuiLayerModule::DeassetViewerPanel::recordSavedDocumentChanges(ImGuiLayerModule& owner)
{
	vector<string> changedKeys = {};
	changedKeys.reserve(document.valueByKey.size() + loadedDocument.valueByKey.size());
	for (auto keyValueIterator = document.valueByKey.begin(); keyValueIterator != document.valueByKey.end(); ++keyValueIterator)
	{
		const string* loadedValue = loadedDocument.find(keyValueIterator->first);
		if (loadedValue != nullptr && *loadedValue == keyValueIterator->second)
		{
			continue;
		}

		changedKeys.push_back(keyValueIterator->first);
	}

	for (auto keyValueIterator = loadedDocument.valueByKey.begin(); keyValueIterator != loadedDocument.valueByKey.end(); ++keyValueIterator)
	{
		if (document.find(keyValueIterator->first) != nullptr)
		{
			continue;
		}

		changedKeys.push_back(keyValueIterator->first);
	}

	std::sort(changedKeys.begin(), changedKeys.end());
	changedKeys.erase(std::unique(changedKeys.begin(), changedKeys.end()), changedKeys.end());
	const string replayAssetPath = displayPathText.empty() ? absoluteFilePath : displayPathText;
	for (uint32 changedKeyIndex = 0; changedKeyIndex < static_cast<uint32>(changedKeys.size()); ++changedKeyIndex)
	{
		const string& changedKey = changedKeys[changedKeyIndex];
		const string* changedValue = document.find(changedKey);
		owner.recordEditorReplayCommand(
			"Editor.setDeassetProperty",
			{ replayAssetPath, changedKey, changedValue != nullptr ? *changedValue : "" });
	}

	loadedDocument = document;
}

void ImGuiLayerModule::DeassetViewerPanel::rebuildDocumentKeys()
{
	documentKeys.clear();
	documentKeys.reserve(document.valueByKey.size());
	for (auto keyValueIterator = document.valueByKey.begin(); keyValueIterator != document.valueByKey.end(); ++keyValueIterator)
	{
		documentKeys.push_back(keyValueIterator->first);
	}

	std::sort(documentKeys.begin(), documentKeys.end());
}

void ImGuiLayerModule::DeassetViewerPanel::build(ImGuiLayerModule& owner, World* world)
{
	unused(world);

	if (!opened)
	{
		return;
	}

	const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	const float windowWidth = clampFloat(
		mainViewport != nullptr ? mainViewport->Size.x * 0.34f : 720.0f,
		520.0f,
		960.0f);
	const float windowHeight = clampFloat(
		mainViewport != nullptr ? mainViewport->Size.y * 0.62f : 720.0f,
		420.0f,
		960.0f);
	ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_FirstUseEver);

	const char* windowTitle = dirty
		? "Deasset Viewer*###DeassetViewer"
		: "Deasset Viewer###DeassetViewer";
	if (!ImGui::Begin(windowTitle, &opened))
	{
		ImGui::End();
		if (!opened)
		{
			reset();
		}
		return;
	}

	const string* assetType = document.find("deasset.@type");
	ImGui::Text("Path: %s", displayPathText.empty() ? absoluteFilePath.c_str() : displayPathText.c_str());
	ImGui::Text("Type: %s", assetType != nullptr ? assetType->c_str() : "(unknown)");
	ImGui::Text("Property Count: %u", static_cast<uint32>(documentKeys.size()));
	if (!statusText.empty())
	{
		ImGui::Text("Status: %s", statusText.c_str());
	}

	if (ImGui::Button("Reload"))
	{
		if (reloadDocument())
		{
			owner.lastEditorActionStatus = "deasset_reloaded";
		}
		else
		{
			owner.lastEditorActionStatus = "deasset_reload_failed";
		}
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!dirty);
	if (ImGui::Button("Save"))
	{
		if (saveDocument())
		{
			recordSavedDocumentChanges(owner);
			owner.lastEditorActionStatus = "deasset_saved";
		}
		else
		{
			owner.lastEditorActionStatus = "deasset_save_failed";
		}
	}
	ImGui::EndDisabled();

	ImGui::Separator();
	if (ImGui::BeginChild("DeassetPropertyList", ImVec2(0.0f, 0.0f), false))
	{
		const ImGuiTableFlags tableFlags =
			ImGuiTableFlags_RowBg
			| ImGuiTableFlags_BordersInnerV
			| ImGuiTableFlags_Resizable
			| ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("DeassetProperties", 2, tableFlags))
		{
			ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.44f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.56f);
			ImGui::TableHeadersRow();

			for (uint32 propertyIndex = 0; propertyIndex < static_cast<uint32>(documentKeys.size()); ++propertyIndex)
			{
				const string& propertyKey = documentKeys[propertyIndex];
				auto propertyIterator = document.valueByKey.find(propertyKey);
				assert(propertyIterator != document.valueByKey.end() && "[ImGuiLayerModule][Assert] reason=deasset_property_missing");

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(propertyKey.c_str());

				ImGui::TableSetColumnIndex(1);
				ImGui::PushID(propertyKey.c_str());
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::InputText("##Value", &propertyIterator->second))
				{
					dirty = true;
					statusText = "modified";
					owner.lastEditorActionStatus = "deasset_modified";
				}
				ImGui::PopID();
			}

			ImGui::EndTable();
		}
	}
	ImGui::EndChild();

	ImGui::End();
	if (!opened)
	{
		reset();
	}
}

void ImGuiLayerModule::FileSystemPanel::reset()
{
	resourcesRootPathText.clear();
	resourcesRootResolved = false;
	resourcesRootValid = false;
	createWorldNameText = "NewWorld";
	lastOpenedWorldPath.clear();
	supportedImportExtensions.clear();
	supportedImportExtensionsLoaded = false;
}

bool ImGuiLayerModule::FileSystemPanel::isDeassetDocumentFile(const filesystem_path& filePath) const
{
	string extension = filePath.extension().string();
	tolower(extension);
	return extension == ".deasset";
}

void ImGuiLayerModule::FileSystemPanel::build(ImGuiLayerModule& owner, World* world)
{
	unused(world);

	owner.applyAnchoredPanelLayout(ImGuiLayerModule::AnchoredPanelSlot::bottom);
	if (!ImGui::Begin("FileSystem", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
	{
		ImGui::End();
		return;
	}

	if (!resolveResourcesRootPath())
	{
		ImGui::TextWrapped("Failed to locate Engine/Resources from current working directory.");
		ImGui::End();
		return;
	}

	ImGui::Text("Root: %s", resourcesRootPathText.c_str());

	const filesystem_path resourcesRootPath(resourcesRootPathText);
	error_code verifyErrorCode;
	if (!exists(resourcesRootPath, verifyErrorCode)
		|| !is_directory(resourcesRootPath, verifyErrorCode))
	{
		resourcesRootResolved = false;
		resourcesRootValid = false;
		ImGui::TextUnformatted("Resources directory is not available.");
		ImGui::End();
		return;
	}

	if (ImGui::Button("+"))
	{
		ImGui::OpenPopup("CreateWorldPopup");
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("CreateWorld");
	}
	ImGui::SameLine();
	ImGui::TextUnformatted("CreateWorld");

	bool closeCreateWorldPopup = false;
	if (ImGui::BeginPopupModal("CreateWorldPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("Create new world file");
		ImGui::InputText("World Name", &createWorldNameText);
		if (ImGui::Button("Create"))
		{
			string createdWorldPath = {};
			if (createWorldFile(owner, createWorldNameText, createdWorldPath))
			{
				if (owner.frameworkReference != nullptr
					&& owner.frameworkReference->loadWorld(createdWorldPath) != nullptr)
				{
					lastOpenedWorldPath = createdWorldPath;
					owner.lastEditorActionStatus = "world_created_loaded_and_saved";
					owner.selectedEntityIndex = invalidEntityIndex;
					World* createdWorld = owner.frameworkReference->getActiveWorld();
					if (createdWorld != nullptr)
					{
						owner.recordEditorReplayCommand("Editor.createWorld", { createdWorld->getName(), createdWorldPath });
					}
				}
				else
				{
					owner.lastEditorActionStatus = "world_created_but_load_failed";
				}
			}
			else
			{
				owner.lastEditorActionStatus = "world_create_failed";
			}

			closeCreateWorldPopup = true;
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			closeCreateWorldPopup = true;
		}

		if (closeCreateWorldPopup)
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::TreeNodeEx("ResourcesRoot", ImGuiTreeNodeFlags_DefaultOpen, "Resources"))
	{
		drawDirectoryEntriesRecursive(owner, resourcesRootPath);
		ImGui::TreePop();
	}

	if (!lastOpenedWorldPath.empty())
	{
		ImGui::Separator();
		ImGui::Text("Last Opened: %s", lastOpenedWorldPath.c_str());
	}
	if (!owner.lastEditorActionStatus.empty())
	{
		ImGui::Text("Status: %s", owner.lastEditorActionStatus.c_str());
	}

	ImGui::End();
}

void ImGuiLayerModule::FileSystemPanel::drawDirectoryEntriesRecursive(
	ImGuiLayerModule& owner,
	const filesystem_path& directoryPath)
{
	vector<filesystem_directory_entry> directoryEntries;
	error_code iterateErrorCode;
	for (const filesystem_directory_entry& directoryEntry : filesystem_directory_iterator(
		directoryPath,
		filesystem_directory_options::skip_permission_denied,
		iterateErrorCode))
	{
		directoryEntries.push_back(directoryEntry);
	}

	if (iterateErrorCode)
	{
		ImGui::Text("Failed to enumerate: %s", directoryPath.string().c_str());
		return;
	}

	if (directoryEntries.empty())
	{
		ImGui::TextUnformatted("(empty)");
		return;
	}

	sortDirectoryEntries(directoryEntries);
	for (const filesystem_directory_entry& directoryEntry : directoryEntries)
	{
		const string fileName = directoryEntry.path().filename().string();
		const string displayName = fileName.empty() ? directoryEntry.path().string() : fileName;
		if (isDirectoryEntry(directoryEntry))
		{
			ImGui::PushID(directoryEntry.path().string().c_str());
			const bool isNodeOpened = ImGui::TreeNodeEx(
				"Directory",
				ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick,
				"%s",
				displayName.c_str());
			if (isNodeOpened)
			{
				drawDirectoryEntriesRecursive(owner, directoryEntry.path());
				ImGui::TreePop();
			}
			ImGui::PopID();
			continue;
		}

		string extension = directoryEntry.path().extension().string();
		tolower(extension);
		if (extension != ".deasset" && extension != ".de")
		{
			continue;
		}

		ImGui::PushID(directoryEntry.path().string().c_str());
		ImGui::Selectable(displayName.c_str(), false);
		drawFileEntryContextMenu(owner, directoryEntry.path());
		const bool deassetDocumentFile = isDeassetDocumentFile(directoryEntry.path());
		const bool worldFileDoubleClicked =
			ImGui::IsItemHovered()
			&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
			&& deassetDocumentFile
			&& isWorldAssetFile(directoryEntry.path());
		if (worldFileDoubleClicked)
		{
			const string worldAssetPath = buildResourceAssetPath(directoryEntry.path());
			if (owner.frameworkReference != nullptr
				&& owner.frameworkReference->loadWorld(worldAssetPath) != nullptr)
			{
				lastOpenedWorldPath = worldAssetPath;
				owner.selectedEntityIndex = invalidEntityIndex;
				owner.lastEditorActionStatus = "world_loaded";
				owner.recordEditorReplayCommand("Editor.loadWorld", { worldAssetPath });
			}
			else
			{
				owner.lastEditorActionStatus = "world_load_failed";
			}
		}
		else if (ImGui::IsItemHovered()
			&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
			&& deassetDocumentFile)
		{
			const string assetPath = buildResourceAssetPath(directoryEntry.path());
			deassetViewerPanel.open(directoryEntry.path(), assetPath);
			owner.lastEditorActionStatus = "deasset_viewer_opened";
		}

		ImGui::PopID();
	}
}

bool ImGuiLayerModule::FileSystemPanel::ensureImportSupportedExtensionsLoaded()
{
	if (supportedImportExtensionsLoaded)
	{
		return true;
	}

	const bool validResourcesRoot = resolveResourcesRootPath();
	assert(validResourcesRoot && "[ImGuiLayerModule][Assert] reason=import_extension_resources_root_resolve_failed");

	XML& xml = XML::get();

	const string importConfigFilePath = (filesystem_path(resourcesRootPathText) / "Config" / "ImportExtensions.xml")
		.lexically_normal()
		.string();
	const XMLKeyValueDocument importDocument = xml.readDocumentFile(importConfigFilePath);

	supportedImportExtensions.clear();
	supportedImportExtensions.reserve(importDocument.valueByKey.size());
	for (auto importEntryIterator = importDocument.valueByKey.begin();
		importEntryIterator != importDocument.valueByKey.end();
		++importEntryIterator)
	{
		if (!importEntryIterator->first.starts_with("extension"))
		{
			continue;
		}

		string extension = importEntryIterator->second;
		tolower(extension);
		if (extension.empty())
		{
			continue;
		}

		if (extension[0] != '.')
		{
			extension.insert(extension.begin(), '.');
		}

		supportedImportExtensions.push_back(moveValue(extension));
	}

	std::sort(supportedImportExtensions.begin(), supportedImportExtensions.end());
	supportedImportExtensions.erase(
		std::unique(supportedImportExtensions.begin(), supportedImportExtensions.end()),
		supportedImportExtensions.end());
	const bool hasSupportedImportExtensions = !supportedImportExtensions.empty();
	assert(hasSupportedImportExtensions && "[ImGuiLayerModule][Assert] reason=import_extension_config_empty");

	supportedImportExtensionsLoaded = true;
	return true;
}

bool ImGuiLayerModule::FileSystemPanel::isImportSupportedFile(const filesystem_path& filePath)
{
	ensureImportSupportedExtensionsLoaded();

	string extension = filePath.extension().string();
	tolower(extension);
	for (size_t extensionIndex = 0; extensionIndex < supportedImportExtensions.size(); ++extensionIndex)
	{
		if (supportedImportExtensions[extensionIndex] == extension)
		{
			return true;
		}
	}

	return false;
}

void ImGuiLayerModule::FileSystemPanel::drawFileEntryContextMenu(
	ImGuiLayerModule& owner,
	const filesystem_path& filePath)
{
	if (!ImGui::BeginPopupContextItem("FileContextMenu"))
	{
		return;
	}

	if (isDeassetDocumentFile(filePath) && ImGui::MenuItem("View Deasset"))
	{
		const string assetPath = buildResourceAssetPath(filePath);
		deassetViewerPanel.open(filePath, assetPath);
		owner.lastEditorActionStatus = "deasset_viewer_opened";
	}

	const bool importSupported = isImportSupportedFile(filePath);
	if (ImGui::MenuItem("Import", nullptr, false, importSupported))
	{
		importPanel.open(filePath);
	}

	ImGui::EndPopup();
}

bool ImGuiLayerModule::FileSystemPanel::createWorldFile(
	ImGuiLayerModule& owner,
	const string& requestedWorldName,
	string& outWorldFilePath)
{
	outWorldFilePath.clear();
	if (!resolveResourcesRootPath() || owner.frameworkReference == nullptr)
	{
		return false;
	}

	shared_pointer<DiskLoaderModule> diskLoaderModule = DiskLoaderModule::get();
	const string worldName = diskLoaderModule->sanitizeFileName(requestedWorldName, "NewWorld");
	const string worldDirectoryPath = (filesystem_path(resourcesRootPathText) / "Scenes")
		.lexically_normal()
		.string();
	string targetWorldPath = {};
	if (!diskLoaderModule->resolveUniqueFilePath(
		worldDirectoryPath,
		worldName,
		".deasset",
		targetWorldPath))
	{
		return false;
	}

	World* world = owner.frameworkReference->createWorld(worldName);
	if (world == nullptr)
	{
		return false;
	}

	const string worldAssetPath = buildResourceAssetPath(targetWorldPath);
	world->setAssetPath(worldAssetPath);
	if (!owner.saveActiveWorldImmediate())
	{
		return false;
	}

	outWorldFilePath = worldAssetPath;
	return true;
}

bool ImGuiLayerModule::saveActiveWorldImmediate()
{
	if (!CLIModule::execute("Framework.saveActiveWorld"))
	{
		return false;
	}

	return CLIModule::get()->getLastExecutionCode() == static_cast<int32>(CLIModule::ExecutionCode::succeeded);
}

bool ImGuiLayerModule::recordEditorReplayCommand(const string& commandName, const vector<string>& arguments) const
{
	const bool appendedCommand = EditorCommandReplay::appendCommand(commandName, arguments);
	if (!appendedCommand)
	{
		error << "[EditorReplayLog] action=append_failed command=" << commandName << lineBreak;
	}

	return appendedCommand;
}

bool ImGuiLayerModule::recordEditorReplayCommandText(const string& commandText) const
{
	const bool appendedCommand = EditorCommandReplay::appendCommandText(commandText);
	if (!appendedCommand)
	{
		error << "[EditorReplayLog] action=append_failed command=" << commandText << lineBreak;
	}

	return appendedCommand;
}

string ImGuiLayerModule::FileSystemPanel::buildResourceAssetPath(const filesystem_path& filePath) const
{
	if (resourcesRootPathText.empty())
	{
		return filePath.lexically_normal().string();
	}

	const filesystem_path relativePath = filePath.lexically_relative(filesystem_path(resourcesRootPathText));
	const string relativePathText = relativePath.lexically_normal().string();
	if (!relativePathText.empty() && !relativePathText.starts_with(".."))
	{
		return relativePathText;
	}

	return filePath.lexically_normal().string();
}

bool ImGuiLayerModule::FileSystemPanel::isWorldAssetFile(const filesystem_path& filePath) const
{
	if (!isDeassetDocumentFile(filePath))
	{
		return false;
	}

	const XMLKeyValueDocument document = XML::get().readDocumentFile(buildResourceAssetPath(filePath));
	const string* assetTypeName = document.find("deasset.@type");
	return assetTypeName != nullptr && *assetTypeName == "World";
}

bool ImGuiLayerModule::FileSystemPanel::resolveResourcesRootPath()
{
	if (resourcesRootResolved)
	{
		return resourcesRootValid;
	}

	resourcesRootResolved = true;
	resourcesRootPathText.clear();
	resourcesRootValid = DiskLoaderModule::get()->resolveResourcesRootPath(resourcesRootPathText);
	if (!resourcesRootValid)
	{
		resourcesRootPathText.clear();
	}

	return resourcesRootValid;
}
