#include "Engine/Module/UI/ImGuiLayerModule.h"

#include "Engine/Framework/Entity.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Framework/FrameworkFileSystem.h"
#include "Engine/Framework/FrameworkSerialization.h"
#include "Engine/Framework/MeshComponent.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/World.h"
#include "Engine/Module/Asset/MeshStreaming.h"
#include "Engine/Module/Render/RenderBackendModule.h"
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
	std::error_code errorCode;
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

ImGuiLayerModule::~ImGuiLayerModule() = default;

bool ImGuiLayerModule::init(Framework& framework)
{
	shutdown();

	frameworkReference = &framework;
	selectedEntityIndex = invalidEntityIndex;
	resourcesRootPathText.clear();
	resourcesRootResolved = false;
	resourcesRootValid = false;
	currentUiScale = 1.0f;
	uiScaleInitialized = false;
	createWorldNameText = "NewWorld";
	lastOpenedWorldPath.clear();
	lastEditorActionStatus.clear();

	if (framework.getExecutionFlow() != FrameworkExecutionFlow::worldFlow)
	{
		return true;
	}

	if (!initializeContext())
	{
		return false;
	}

	WindowsWindowObject* windowObject = framework.getWindowObject();
	shared_pointer<RenderBackendModule> renderBackendModule = RenderBackendModule::get();
	if (windowObject == nullptr
		|| renderBackendModule == nullptr
		|| !renderBackendModule->isBackendCreated())
	{
		error << "ImGuiLayerModule init failed. reason=worldflow_prerequisite_missing" << lineBreak;
		shutdown();
		return false;
	}

	if (!ImGui_ImplWin32_Init(windowObject->getWindowHandle()))
	{
		error << "ImGuiLayerModule init failed. reason=win32_backend_init_failed" << lineBreak;
		shutdown();
		return false;
	}

	win32BackendInitialized = true;

	if (framework.getBackendOptions().backendType != RenderBackendType::dx12)
	{
		output << "[ImGuiLayer][Warn] backend_not_supported_for_imgui" << lineBreak;
		return true;
	}

	RenderBackend* renderBackend = renderBackendModule->getBackend();
	if (renderBackend == nullptr)
	{
		error << "ImGuiLayerModule init failed. reason=render_backend_missing" << lineBreak;
		shutdown();
		return false;
	}

	backendBridge.reset(new Dx12BackendBridge());
	if (backendBridge == nullptr || !backendBridge->initialize(*renderBackend))
	{
		error << "ImGuiLayerModule init failed. reason=dx12_backend_init_failed" << lineBreak;
		shutdown();
		return false;
	}

	updateUiScaleIfNeeded();
	return true;
}

void ImGuiLayerModule::preUpdate()
{
	if (frameworkReference == nullptr
		|| frameworkReference->getExecutionFlow() != FrameworkExecutionFlow::worldFlow)
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
	resourcesRootPathText.clear();
	resourcesRootResolved = false;
	resourcesRootValid = false;
	currentUiScale = 1.0f;
	uiScaleInitialized = false;
	createWorldNameText = "NewWorld";
	lastOpenedWorldPath.clear();
	lastEditorActionStatus.clear();
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
		|| frameworkReference->getExecutionFlow() != FrameworkExecutionFlow::worldFlow)
	{
		return false;
	}

	return ImGui_ImplWin32_WndProcHandler(
		windowHandle,
		messageIdentifier,
		firstParameter,
		secondParameter) != 0;
}

void ImGuiLayerModule::buildAndRender(CommandList* commandList)
{
	if (commandList == nullptr
		|| !contextCreated
		|| !win32BackendInitialized
		|| backendBridge == nullptr
		|| frameworkReference == nullptr)
	{
		return;
	}

	if (!backendBridge->beginFrame())
	{
		return;
	}

	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	World* world = frameworkReference->getActiveWorld();
	buildOutlinerPanel(world);
	buildDetailPanel(world);
	buildFileSystemPanel();

	ImGui::Render();
	backendBridge->renderDrawData(commandList);
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

void ImGuiLayerModule::buildOutlinerPanel(World* world)
{
	ImGui::Begin("Outliner");

	if (world == nullptr)
	{
		ImGui::TextUnformatted("No active world.");
		ImGui::End();
		return;
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
		if (selectedEntityIndex != invalidEntityIndex
			&& world->getEntityByIndex(selectedEntityIndex) != nullptr)
		{
			addEntityResult = world->addChildEntity(selectedEntityIndex, newEntityIndex);
		}

		selectedEntityIndex = newEntityIndex;
		if (!addEntityResult)
		{
			lastEditorActionStatus = "add_entity_failed";
		}
		else if (saveActiveWorldImmediate())
		{
			lastEditorActionStatus = "entity_added_and_saved";
		}
		else
		{
			lastEditorActionStatus = "entity_added_save_skipped";
		}
	}

	const uint32 entityCount = world->getEntityCount();
	ImGui::Text("Entity Count: %u", entityCount);

	uint32 rootEntityCount = 0;
	for (uint32 entityIndex = 0; entityIndex < entityCount; ++entityIndex)
	{
		const Entity* entity = world->getEntityByIndex(entityIndex);
		if (entity == nullptr
			|| entity->parentEntityIndex != invalidEntityIndex)
		{
			continue;
		}

		drawOutlinerEntityNode(world, entityIndex);
		++rootEntityCount;
	}

	if (rootEntityCount == 0)
	{
		ImGui::TextUnformatted("No root entities.");
	}

	if (!lastEditorActionStatus.empty())
	{
		ImGui::Separator();
		ImGui::Text("Status: %s", lastEditorActionStatus.c_str());
	}

	ImGui::End();
}

void ImGuiLayerModule::drawOutlinerEntityNode(const World* world, const uint32 entityIndex)
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

	ImGui::PushID(static_cast<int32>(entityIndex));

	ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (selectedEntityIndex == entityIndex)
	{
		treeNodeFlags |= ImGuiTreeNodeFlags_Selected;
	}
	if (entity->firstChildEntityIndex == invalidEntityIndex)
	{
		treeNodeFlags |= ImGuiTreeNodeFlags_Leaf;
	}

	const bool isNodeOpened = ImGui::TreeNodeEx(
		"Entity",
		treeNodeFlags,
		"Entity [%u]",
		entityIndex);
	if (ImGui::IsItemClicked())
	{
		selectedEntityIndex = entityIndex;
	}

	if (isNodeOpened)
	{
		uint32 childEntityIndex = entity->firstChildEntityIndex;
		uint32 childGuardCount = 0;
		const uint32 maxChildGuardCount = world->getEntityCount();
		while (childEntityIndex != invalidEntityIndex && childGuardCount < maxChildGuardCount)
		{
			drawOutlinerEntityNode(world, childEntityIndex);
			const Entity* childEntity = world->getEntityByIndex(childEntityIndex);
			if (childEntity == nullptr)
			{
				break;
			}

			childEntityIndex = childEntity->nextSiblingEntityIndex;
			++childGuardCount;
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

void ImGuiLayerModule::buildDetailPanel(World* world)
{
	ImGui::Begin("Detail");

	if (world == nullptr)
	{
		ImGui::TextUnformatted("No active world.");
		ImGui::End();
		return;
	}

	if (selectedEntityIndex == invalidEntityIndex)
	{
		ImGui::TextUnformatted("Select an entity from Outliner.");
		ImGui::End();
		return;
	}

	Entity* selectedEntity = world->getEntityByIndex(selectedEntityIndex);
	if (selectedEntity == nullptr)
	{
		selectedEntityIndex = invalidEntityIndex;
		ImGui::TextUnformatted("Selected entity is invalid.");
		ImGui::End();
		return;
	}

	ImGui::Text("Entity [%u]", selectedEntityIndex);
	ImGui::Text("Active: %s", selectedEntity->active ? "true" : "false");
	if (selectedEntity->parentEntityIndex == invalidEntityIndex)
	{
		ImGui::TextUnformatted("Parent: invalid");
	}
	else
	{
		ImGui::Text("Parent: %u", selectedEntity->parentEntityIndex);
	}
	if (selectedEntity->firstChildEntityIndex == invalidEntityIndex)
	{
		ImGui::TextUnformatted("First Child: invalid");
	}
	else
	{
		ImGui::Text("First Child: %u", selectedEntity->firstChildEntityIndex);
	}
	if (selectedEntity->nextSiblingEntityIndex == invalidEntityIndex)
	{
		ImGui::TextUnformatted("Next Sibling: invalid");
	}
	else
	{
		ImGui::Text("Next Sibling: %u", selectedEntity->nextSiblingEntityIndex);
	}

	ImGui::Separator();
	const uint32 componentCount = selectedEntity->getComponentCount();
	ImGui::Text("Components: %u", componentCount);

	MeshComponent* meshComponent = nullptr;
	for (uint32 componentArrayIndex = 0; componentArrayIndex < componentCount; ++componentArrayIndex)
	{
		const uint32 componentIndex = selectedEntity->getComponentIndex(componentArrayIndex);
		ImGui::BulletText("Component Index: %u", componentIndex);

		Component* component = world->getComponentByIndex(componentIndex);
		MeshComponent* currentMeshComponent = dynamic_cast<MeshComponent*>(component);
		if (currentMeshComponent != nullptr && meshComponent == nullptr)
		{
			meshComponent = currentMeshComponent;
		}
	}

	ImGui::Separator();
	const bool hasMeshComponent = meshComponent != nullptr;
	ImGui::BeginDisabled(hasMeshComponent);
	if (ImGui::Button("+ AddComponent: MeshComponent"))
	{
		unique_pointer<MeshComponent> newMeshComponent(new MeshComponent());
		if (selectedEntity->addComponent(moveValue(newMeshComponent)))
		{
			lastEditorActionStatus = saveActiveWorldImmediate()
				? "mesh_component_added_and_saved"
				: "mesh_component_added_save_skipped";
		}
		else
		{
			lastEditorActionStatus = "mesh_component_add_failed";
		}
	}
	ImGui::EndDisabled();

	if (hasMeshComponent)
	{
		ImGui::TextUnformatted("MeshComponent");

		string meshPath = meshComponent->meshRelativePath;
		int32 lodLevel = static_cast<int32>(meshComponent->lodLevel);
		bool visible = meshComponent->visible;
		bool meshComponentChanged = false;

		if (ImGui::InputText("Mesh Path", &meshPath))
		{
			meshComponent->meshRelativePath = meshPath;
			meshComponentChanged = true;
		}

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
			if (!meshComponent->meshRelativePath.empty())
			{
				MeshStreaming::get()->requestMesh(
					meshComponent->meshRelativePath,
					meshComponent->lodLevel);
			}

			lastEditorActionStatus = saveActiveWorldImmediate()
				? "mesh_component_updated_and_saved"
				: "mesh_component_updated_save_skipped";
		}
	}

	const PlaceableEntity* placeableEntity = dynamic_cast<const PlaceableEntity*>(selectedEntity);
	if (placeableEntity != nullptr)
	{
		const Transform& transform = placeableEntity->transform;
		ImGui::Separator();
		ImGui::TextUnformatted("Transform");
		ImGui::Text("Position: %.3f, %.3f, %.3f", transform.positionX, transform.positionY, transform.positionZ);
		ImGui::Text("Rotation: %.3f, %.3f, %.3f", transform.rotationPitch, transform.rotationYaw, transform.rotationRoll);
		ImGui::Text("Scale: %.3f, %.3f, %.3f", transform.scaleX, transform.scaleY, transform.scaleZ);
	}

	ImGui::End();
}

void ImGuiLayerModule::buildFileSystemPanel()
{
	ImGui::Begin("FileSystem");

	if (!resolveResourcesRootPath())
	{
		ImGui::TextWrapped("Failed to locate Engine/Resources from current working directory.");
		ImGui::End();
		return;
	}

	ImGui::Text("Root: %s", resourcesRootPathText.c_str());

	const filesystem_path resourcesRootPath(resourcesRootPathText);
	std::error_code verifyErrorCode;
	if (!std::filesystem::exists(resourcesRootPath, verifyErrorCode)
		|| !std::filesystem::is_directory(resourcesRootPath, verifyErrorCode))
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
				if (frameworkReference != nullptr
					&& frameworkReference->loadWorldFromFile(createdWorldPath))
				{
					lastOpenedWorldPath = createdWorldPath;
					lastEditorActionStatus = frameworkReference->saveActiveWorldToFile()
						? "world_created_loaded_and_saved"
						: "world_created_loaded_save_skipped";
					selectedEntityIndex = invalidEntityIndex;
				}
				else
				{
					lastEditorActionStatus = "world_created_but_load_failed";
				}
			}
			else
			{
				lastEditorActionStatus = "world_create_failed";
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
		drawDirectoryEntriesRecursive(resourcesRootPath);
		ImGui::TreePop();
	}

	if (!lastOpenedWorldPath.empty())
	{
		ImGui::Separator();
		ImGui::Text("Last Opened: %s", lastOpenedWorldPath.c_str());
	}
	if (!lastEditorActionStatus.empty())
	{
		ImGui::Text("Status: %s", lastEditorActionStatus.c_str());
	}

	ImGui::End();
}

void ImGuiLayerModule::drawDirectoryEntriesRecursive(const filesystem_path& directoryPath)
{
	vector<filesystem_directory_entry> directoryEntries;
	std::error_code iterateErrorCode;
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
				drawDirectoryEntriesRecursive(directoryEntry.path());
				ImGui::TreePop();
			}
			ImGui::PopID();
			continue;
		}

		ImGui::PushID(directoryEntry.path().string().c_str());
		ImGui::Selectable(displayName.c_str(), false);
		const bool worldFileDoubleClicked =
			ImGui::IsItemHovered()
			&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
			&& directoryEntry.path().extension() == ".world";
		if (worldFileDoubleClicked)
		{
			if (frameworkReference != nullptr
				&& frameworkReference->loadWorldFromFile(directoryEntry.path().string()))
			{
				lastOpenedWorldPath = directoryEntry.path().lexically_normal().string();
				selectedEntityIndex = invalidEntityIndex;
				lastEditorActionStatus = "world_loaded";
			}
			else
			{
				lastEditorActionStatus = "world_load_failed";
			}
		}

		ImGui::PopID();
	}
}

bool ImGuiLayerModule::createWorldFile(const string& requestedWorldName, string& outWorldFilePath)
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

	World newWorld(worldNameWide);
	string saveErrorText = {};
	if (!frameworkSerializationSaveWorldToFile(newWorld, targetWorldPath, saveErrorText))
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

bool ImGuiLayerModule::resolveResourcesRootPath()
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
