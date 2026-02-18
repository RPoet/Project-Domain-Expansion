#include "Engine/Module/UI/ImGuiLayerModule.h"

#include "Engine/Framework/Entity.h"
#include "Engine/Framework/Framework.h"
#include "Engine/Framework/PlaceableEntity.h"
#include "Engine/Framework/World.h"
#include "Engine/Module/Render/RenderBackendModule.h"
#include "Render/Backends/Dx12/Dx12CommandList.h"
#include "Render/Backends/RenderBackend.h"

#include "ThirdParty/ImGui/imgui.h"
#include "ThirdParty/ImGui/backends/imgui_impl_dx12.h"
#include "ThirdParty/ImGui/backends/imgui_impl_win32.h"

#include <algorithm>
#include <d3d12.h>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;
static constexpr int32 dx12FrameBufferCountForImGui = 2;
extern IMGUI_IMPL_API MessageResult ImGui_ImplWin32_WndProcHandler(
	HandleWindow windowHandle,
	MessageIdentifier messageIdentifier,
	MessageFirstParameter firstParameter,
	MessageSecondParameter secondParameter);

static bool isDirectoryEntry(const fs::directory_entry& directoryEntry)
{
	std::error_code errorCode;
	return directoryEntry.is_directory(errorCode) && !errorCode;
}

static void sortDirectoryEntries(vector<fs::directory_entry>& directoryEntries)
{
	std::sort(
		directoryEntries.begin(),
		directoryEntries.end(),
		[](const fs::directory_entry& leftEntry, const fs::directory_entry& rightEntry)
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

static void drawDirectoryEntriesRecursive(const fs::path& directoryPath)
{
	vector<fs::directory_entry> directoryEntries;
	std::error_code iterateErrorCode;
	for (const fs::directory_entry& directoryEntry : fs::directory_iterator(
		directoryPath,
		fs::directory_options::skip_permission_denied,
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
	for (const fs::directory_entry& directoryEntry : directoryEntries)
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

		ImGui::BulletText("%s", displayName.c_str());
	}
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

	return true;
}

void ImGuiLayerModule::update()
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

	const World* world = frameworkReference->getActiveWorld();
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

void ImGuiLayerModule::buildOutlinerPanel(const World* world)
{
	ImGui::Begin("Outliner");

	if (world == nullptr)
	{
		ImGui::TextUnformatted("No active world.");
		ImGui::End();
		return;
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

void ImGuiLayerModule::buildDetailPanel(const World* world)
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

	const Entity* selectedEntity = world->getEntityByIndex(selectedEntityIndex);
	if (selectedEntity == nullptr)
	{
		selectedEntityIndex = invalidEntityIndex;
		ImGui::TextUnformatted("Selected entity is invalid.");
		ImGui::End();
		return;
	}

	ImGui::Text("Entity [%u]", selectedEntityIndex);
	ImGui::Text("Active: %s", selectedEntity->activeState ? "true" : "false");
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
	for (uint32 componentArrayIndex = 0; componentArrayIndex < componentCount; ++componentArrayIndex)
	{
		const uint32 componentIndex = selectedEntity->getComponentIndex(componentArrayIndex);
		ImGui::BulletText("Component Index: %u", componentIndex);
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

	const fs::path resourcesRootPath(resourcesRootPathText);
	std::error_code verifyErrorCode;
	if (!fs::exists(resourcesRootPath, verifyErrorCode)
		|| !fs::is_directory(resourcesRootPath, verifyErrorCode))
	{
		resourcesRootResolved = false;
		resourcesRootValid = false;
		ImGui::TextUnformatted("Resources directory is not available.");
		ImGui::End();
		return;
	}

	if (ImGui::TreeNodeEx("ResourcesRoot", ImGuiTreeNodeFlags_DefaultOpen, "Resources"))
	{
		drawDirectoryEntriesRecursive(resourcesRootPath);
		ImGui::TreePop();
	}

	ImGui::End();
}

bool ImGuiLayerModule::resolveResourcesRootPath()
{
	if (resourcesRootResolved)
	{
		return resourcesRootValid;
	}

	resourcesRootResolved = true;
	resourcesRootValid = false;
	resourcesRootPathText.clear();

	std::error_code currentPathErrorCode;
	fs::path currentPath = fs::current_path(currentPathErrorCode);
	if (currentPathErrorCode)
	{
		return false;
	}

	for (uint32 pathDepth = 0; pathDepth < 16; ++pathDepth)
	{
		const fs::path candidatePath = currentPath / "Engine" / "Resources";
		std::error_code candidateErrorCode;
		if (fs::exists(candidatePath, candidateErrorCode)
			&& fs::is_directory(candidatePath, candidateErrorCode))
		{
			resourcesRootPathText = candidatePath.lexically_normal().string();
			resourcesRootValid = true;
			return true;
		}

		const fs::path parentPath = currentPath.parent_path();
		if (parentPath.empty() || parentPath == currentPath)
		{
			break;
		}

		currentPath = parentPath;
	}

	return false;
}
