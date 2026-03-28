#include "Engine/Module/UI/ImGuiLayerModule.h"

#include "Engine/Framework/Entity.h"
#include "Engine/Framework/CameraComponent.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Framework/FrameworkFileSystem.h"
#include "Engine/Framework/FrameworkSerialization.h"
#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/World.h"
#include "Engine/Module/MeshParser/MeshParser.h"
#include "Engine/Module/MeshStreaming/MeshStreaming.h"
#include "Engine/Module/CLI/CLIModule.h"
#include "Engine/Module/DiskLoader/DiskLoaderModule.h"
#include "Engine/Module/ShaderPackage/ShaderPackageModule.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Engine/Common/XML/XML.h"
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

static string buildEntityDisplayText(
	const World* world,
	const Entity* entity,
	const uint32 entityIndex)
{
	string displayText = entity != nullptr && !entity->getName().empty()
		? entity->getName()
		: "Entity";
	displayText += " [";
	displayText += std::to_string(entityIndex);
	displayText += "]";
	if (isEditorCameraEntity(world, entity))
	{
		displayText += " [EditorCamera]";
	}

	return displayText;
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

static string toNarrowText(const wstring& text)
{
	string result = {};
	result.reserve(text.length());
	for (size_t index = 0; index < text.length(); ++index)
	{
		const wide_character character = text[index];
		result.push_back(character >= 0 && character <= 127
			? static_cast<char>(character)
			: '?');
	}

	return result;
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
	, fileSystemPanel(new FileSystemPanel(*importPanel))
{
	panels.push_back(outlinerPanel.get());
	panels.push_back(detailPanel.get());
	panels.push_back(fileSystemPanel.get());
	panels.push_back(importPanel.get());
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

	commandText = "MeshParser.import \"" + sourceFilePathText + "\"";
	CLIModule::execute(commandText);
	shared_pointer<CLIModule> cliModule = CLIModule::get();
	processCode = mapProcessCodeFromCLIExecutionCode(cliModule->getLastExecutionCode());
	processCodeAvailable = true;
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
	const bool validPanels = panels.size() == 4
		&& importPanel != nullptr
		&& outlinerPanel != nullptr
		&& detailPanel != nullptr
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
	const bool validPanels = panels.size() == 4
		&& importPanel != nullptr
		&& outlinerPanel != nullptr
		&& detailPanel != nullptr
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
	const bool validPanels = panels.size() == 4
		&& importPanel != nullptr
		&& outlinerPanel != nullptr
		&& detailPanel != nullptr
		&& fileSystemPanel != nullptr
		&& panels[0] != nullptr
		&& panels[1] != nullptr
		&& panels[2] != nullptr
		&& panels[3] != nullptr;
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
	ImGui::Begin("Outliner");

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

	string worldNameText = toNarrowText(world->getWorldName());
	if (worldNameText.empty())
	{
		worldNameText = "(unnamed)";
	}
	ImGui::Text("World: %s", worldNameText.c_str());

	if (ImGui::Button("+ AddEntity"))
	{
		const uint32 newEntityIndex = world->createPlaceableEntity();
		bool addEntityResult = true;
		if (owner.selectedEntityIndex != invalidEntityIndex
			&& world->getEntityByIndex(owner.selectedEntityIndex) != nullptr)
		{
			addEntityResult = world->addChildEntity(owner.selectedEntityIndex, newEntityIndex);
		}

		owner.selectedEntityIndex = newEntityIndex;
		if (!addEntityResult)
		{
			owner.lastEditorActionStatus = "add_entity_failed";
		}
		else if (owner.saveActiveWorldImmediate())
		{
			owner.lastEditorActionStatus = "entity_added_and_saved";
		}
		else
		{
			owner.lastEditorActionStatus = "entity_added_save_skipped";
		}
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

	const uint32 entityCount = world->getEntityCount();
	ImGui::Text("Entity Count: %u", entityCount);

	uint32 rootEntityCount = 0;
	for (uint32 entityIndex = 0; entityIndex < entityCount; ++entityIndex)
	{
		const Entity* entity = world->getEntityByIndex(entityIndex);
		if (entity == nullptr
			|| entity->getParentEntityIndex() != invalidEntityIndex)
		{
			continue;
		}

		drawEntityNode(owner, world, entityIndex);
		++rootEntityCount;
	}

	if (rootEntityCount == 0)
	{
		ImGui::TextUnformatted("No root entities.");
	}

	if (!owner.lastEditorActionStatus.empty())
	{
		ImGui::Separator();
		ImGui::Text("Status: %s", owner.lastEditorActionStatus.c_str());
	}

	ImGui::End();
}

void ImGuiLayerModule::OutlinerPanel::drawEntityNode(ImGuiLayerModule& owner, const World* world, const uint32 entityIndex)
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

	const bool editorCameraEntity = isEditorCameraEntity(world, entity);

	ImGui::PushID(static_cast<int32>(entityIndex));

	ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (owner.selectedEntityIndex == entityIndex && !editorCameraEntity)
	{
		treeNodeFlags |= ImGuiTreeNodeFlags_Selected;
	}
	if (entity->getFirstChildEntityIndex() == invalidEntityIndex)
	{
		treeNodeFlags |= ImGuiTreeNodeFlags_Leaf;
	}

	const bool isNodeOpened = ImGui::TreeNodeEx(
		"Entity",
		treeNodeFlags,
		"%s",
		buildEntityDisplayText(world, entity, entityIndex).c_str());
	if (ImGui::IsItemClicked() && !editorCameraEntity)
	{
		owner.selectedEntityIndex = entityIndex;
	}

	if (isNodeOpened)
	{
		uint32 childEntityIndex = entity->getFirstChildEntityIndex();
		uint32 childGuardCount = 0;
		const uint32 maxChildGuardCount = world->getEntityCount();
		while (childEntityIndex != invalidEntityIndex && childGuardCount < maxChildGuardCount)
		{
			drawEntityNode(owner, world, childEntityIndex);
			const Entity* childEntity = world->getEntityByIndex(childEntityIndex);
			if (childEntity == nullptr)
			{
				break;
			}

			childEntityIndex = childEntity->getNextSiblingEntityIndex();
			++childGuardCount;
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

void ImGuiLayerModule::DetailPanel::build(ImGuiLayerModule& owner, World* world)
{
	ImGui::Begin("Detail");

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

	const string selectedEntityDisplayText = buildEntityDisplayText(world, selectedEntity, owner.selectedEntityIndex);
	ImGui::Text("%s", selectedEntityDisplayText.c_str());
	string entityName = selectedEntity->getName();
	if (ImGui::InputText("Name", &entityName))
	{
		selectedEntity->setName(entityName);
		owner.lastEditorActionStatus = owner.saveActiveWorldImmediate()
			? "entity_name_updated_and_saved"
			: "entity_name_updated_save_skipped";
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
				owner.lastEditorActionStatus = owner.saveActiveWorldImmediate()
					? "mesh_component_added_and_saved"
					: "mesh_component_added_save_skipped";
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
				owner.lastEditorActionStatus = owner.saveActiveWorldImmediate()
					? "camera_component_added_and_saved"
					: "camera_component_added_save_skipped";
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

		string meshPath = meshComponent->meshAssetPath;
		int32 lodLevel = static_cast<int32>(meshComponent->lodLevel);
		bool visible = meshComponent->visible;
		bool meshComponentChanged = false;

		if (ImGui::InputText("Mesh Asset Path", &meshPath))
		{
			meshComponent->meshAssetPath = meshPath;
			meshComponent->meshAsset.reset();
			if (!meshPath.empty())
			{
				const XMLKeyValueDocument document = XML::get().readDocumentFile(meshPath);
				MeshAsset meshAsset = {};
				meshAsset.setAssetPath(meshPath);
				meshAsset.readProperty(document);
				meshComponent->meshAsset = shared_pointer<MeshAsset>(new MeshAsset(moveValue(meshAsset)));
			}

			meshComponentChanged = true;
		}
		ImGui::TextDisabled("Example: Meshes/Plane.deasset");

		if (ImGui::InputInt("LOD", &lodLevel))
		{
			if (lodLevel < 0)
			{
				lodLevel = 0;
			}

			meshComponent->lodLevel = static_cast<uint32>(lodLevel);
			meshComponentChanged = true;
		}

		if (ImGui::Checkbox("Visible", &visible))
		{
			meshComponent->visible = visible;
			meshComponentChanged = true;
		}

		if (meshComponentChanged)
		{
			if (meshComponent->meshAsset != nullptr)
			{
				MeshStreaming::get()->requestMesh(
					meshComponent->meshAsset,
					meshComponent->lodLevel);
			}

			owner.lastEditorActionStatus = owner.saveActiveWorldImmediate()
				? "mesh_component_updated_and_saved"
				: "mesh_component_updated_save_skipped";
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

			owner.lastEditorActionStatus = owner.saveActiveWorldImmediate()
				? "camera_component_updated_and_saved"
				: "camera_component_updated_save_skipped";
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
			owner.lastEditorActionStatus = owner.saveActiveWorldImmediate()
				? "entity_transform_updated_and_saved"
				: "entity_transform_updated_save_skipped";
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
	if (!world->removeEntity(entityIndexToDelete))
	{
		lastEditorActionStatus = "entity_delete_failed";
		return false;
	}

	selectedEntityIndex = invalidEntityIndex;
	lastEditorActionStatus = saveActiveWorldImmediate()
		? "entity_deleted_and_saved"
		: "entity_deleted_save_skipped";
	return true;
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

void ImGuiLayerModule::FileSystemPanel::build(ImGuiLayerModule& owner, World* world)
{
	unused(world);

	ImGui::Begin("FileSystem");

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
			if (createWorldFile(createWorldNameText, createdWorldPath))
			{
				if (owner.frameworkReference != nullptr
					&& owner.frameworkReference->loadWorldFromFile(createdWorldPath))
				{
					lastOpenedWorldPath = createdWorldPath;
					owner.lastEditorActionStatus = owner.frameworkReference->saveActiveWorldToFile()
						? "world_created_loaded_and_saved"
						: "world_created_loaded_save_skipped";
					owner.selectedEntityIndex = invalidEntityIndex;
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

		ImGui::PushID(directoryEntry.path().string().c_str());
		ImGui::Selectable(displayName.c_str(), false);
		drawFileEntryContextMenu(directoryEntry.path());
		const bool worldFileDoubleClicked =
			ImGui::IsItemHovered()
			&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
			&& directoryEntry.path().extension() == ".world";
		if (worldFileDoubleClicked)
		{
			if (owner.frameworkReference != nullptr
				&& owner.frameworkReference->loadWorldFromFile(directoryEntry.path().string()))
			{
				lastOpenedWorldPath = directoryEntry.path().lexically_normal().string();
				owner.selectedEntityIndex = invalidEntityIndex;
				owner.lastEditorActionStatus = "world_loaded";
			}
			else
			{
				owner.lastEditorActionStatus = "world_load_failed";
			}
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

void ImGuiLayerModule::FileSystemPanel::drawFileEntryContextMenu(const filesystem_path& filePath)
{
	if (!ImGui::BeginPopupContextItem("FileContextMenu"))
	{
		return;
	}

	const bool importSupported = isImportSupportedFile(filePath);
	if (ImGui::MenuItem("Import", nullptr, false, importSupported))
	{
		importPanel.open(filePath);
	}

	ImGui::EndPopup();
}

bool ImGuiLayerModule::FileSystemPanel::createWorldFile(
	const string& requestedWorldName,
	string& outWorldFilePath)
{
	outWorldFilePath.clear();
	if (!resolveResourcesRootPath())
	{
		return false;
	}

	const string worldName = frameworkFileSystemSanitizeFileName(requestedWorldName, "NewWorld");
	const string worldDirectoryPath = (filesystem_path(resourcesRootPathText) / "Scenes")
		.lexically_normal()
		.string();
	string targetWorldPath = {};
	if (!frameworkFileSystemResolveUniqueFilePath(
		worldDirectoryPath,
		worldName,
		".world",
		targetWorldPath))
	{
		return false;
	}

	wstring worldNameWide = {};
	worldNameWide.reserve(worldName.length());
	for (size_t characterIndex = 0; characterIndex < worldName.length(); ++characterIndex)
	{
		worldNameWide.push_back(static_cast<wide_character>(static_cast<unsigned char>(worldName[characterIndex])));
	}

	string editorWorldTemplatePath = {};
	if (!frameworkFileSystemResolveEditorWorldTemplateFilePath(editorWorldTemplatePath))
	{
		return false;
	}

	unique_pointer<World> newWorld = nullptr;
	string loadErrorText = {};
	if (!frameworkSerializationLoadWorldFromFile(editorWorldTemplatePath, newWorld, loadErrorText)
		|| newWorld == nullptr)
	{
		unused(loadErrorText);
		return false;
	}

	newWorld->setWorldName(worldNameWide);
	string saveErrorText = {};
	if (!frameworkSerializationSaveWorldToFile(*newWorld, targetWorldPath, saveErrorText))
	{
		unused(saveErrorText);
		return false;
	}

	outWorldFilePath = filesystem_path(targetWorldPath).lexically_normal().string();
	return true;
}

bool ImGuiLayerModule::saveActiveWorldImmediate()
{
	if (frameworkReference == nullptr)
	{
		return false;
	}

	if (frameworkReference->getActiveWorldFilePath().empty())
	{
		return false;
	}

	return frameworkReference->saveActiveWorldToFile();
}

bool ImGuiLayerModule::FileSystemPanel::resolveResourcesRootPath()
{
	if (resourcesRootResolved)
	{
		return resourcesRootValid;
	}

	resourcesRootResolved = true;
	resourcesRootPathText.clear();
	resourcesRootValid = frameworkFileSystemResolveResourcesRootPath(resourcesRootPathText);
	if (!resourcesRootValid)
	{
		resourcesRootPathText.clear();
	}

	return resourcesRootValid;
}
