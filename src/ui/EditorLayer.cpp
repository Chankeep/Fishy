#include "EditorLayer.h"

#include "core/LogSystem.h"
#include "ecs/components/HierarchyComponent.h"
#include "ecs/components/TagComponent.h"
#include "ecs/components/TransformComponent.h"
#include "scene/Scene.h"

#include <cstring>
#include <imgui.h>
#include <imgui_internal.h> // for DockBuilder API

namespace Fishy {
void EditorLayer::init(VulkanDevice& device, vk::Format colorFormat) {
	// Allocate scene framebuffer that ImGui will present
	_sceneFramebuffer = std::make_unique<SceneFramebuffer>(device, colorFormat);
	FISHY_LOG_INFO("EditorLayer initialized");
}

void EditorLayer::shutdown() {
	// Release the underlying Vulkan resources associated with the framebuffer
	_sceneFramebuffer.reset();
	FISHY_LOG_INFO("EditorLayer shut down");
}

void EditorLayer::beginFrame() {
	// Dynamically resize the scene framebuffer if the viewport dimensions change
	auto w = static_cast<uint32_t>(_viewportSize.x);
	auto h = static_cast<uint32_t>(_viewportSize.y);
	if (w > 0 && h > 0 && _sceneFramebuffer->needsResize(w, h)) {
		_sceneFramebuffer->resize(w, h);
	}
}

void EditorLayer::onImGui(Scene& scene) {
	// Dispatch function calls to draw the different UI panels
	drawDockSpace();
	drawViewport();
	drawLogWindow();
	drawSceneHierarchy(scene);
	drawContentBrowser();
	drawPropertiesPanel(scene);
}

// ─────────────────── DockSpace ───────────────────

void EditorLayer::drawDockSpace() {
	// Set up the outer dockspace window to fill the main viewport
	ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
							 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
							 ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	// Remove styling to seamlessly embed into the main window
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("DockSpace", nullptr, flags);
	
	// Standard main menu bar (currently just placeholders)
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("file")) {
			if (ImGui::MenuItem("Save")) {
			}
			if (ImGui::MenuItem("Load")) {
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::PopStyleVar(3);

	ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");

	// Perform one-time setup of layout
	if (_firstFrame) {
		setupDefaultLayout(dockspaceId);
		_firstFrame = false;
	}

	// Apply dockspace
	ImGui::DockSpace(dockspaceId, ImVec2(0, 0), ImGuiDockNodeFlags_None);
	ImGui::End();
}

void EditorLayer::setupDefaultLayout(ImGuiID dockspaceId) {
	// Programmatically construct the docking layout hierarchy
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

	ImGuiID centerId = dockspaceId;

	// Split nodes to allocate space for panels
	ImGuiID leftId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Left, 0.2f, nullptr, &centerId);
	ImGuiID leftBottomId = ImGui::DockBuilderSplitNode(leftId, ImGuiDir_Down, 0.3f, nullptr, &leftId);

	ImGuiID rightId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.25f, nullptr, &centerId);

	ImGuiID bottomId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, 0.3f, nullptr, &centerId);

	// Explicitly dock target windows into the divided sections
	ImGui::DockBuilderDockWindow("Scene Hierarchy", leftId);
	ImGui::DockBuilderDockWindow("Shader Debug", leftBottomId);
	ImGui::DockBuilderDockWindow("Properties", rightId);
	ImGui::DockBuilderDockWindow("Viewport", centerId);

	ImGui::DockBuilderDockWindow("Content Browser", bottomId);
	ImGui::DockBuilderDockWindow("Log", bottomId);

	ImGui::DockBuilderFinish(dockspaceId);
}

// ─────────────────── Viewport ───────────────────

void EditorLayer::drawViewport() {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::Begin("Viewport");

	_viewportFocused = ImGui::IsWindowFocused();
	_viewportHovered = ImGui::IsWindowHovered();

	ImVec2 size = ImGui::GetContentRegionAvail();
	if (size.x > 0 && size.y > 0) {
		_viewportSize = size;

		// NOTE: Do NOT resize here! Resize happens before renderToTexture()
		// to avoid invalidating in-flight command buffers.
		ImGui::Image(reinterpret_cast<ImTextureID>(_sceneFramebuffer->getImGuiTextureID()), size);
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

// ─────────────────── Scene Hierarchy ───────────────────

void EditorLayer::drawSceneHierarchy(Scene& scene) {
	ImGui::Begin("Scene Hierarchy");

	auto& registry = scene.getRegistry();
	auto view = registry.view<TagComponent>();

	// Iterate all tagged entities to build the upper level of the hierarchy
	for (auto entity : view) {
		auto* hierarchy = registry.try_get<HierarchyComponent>(entity);

		// We only process top-level entities here; children are processed recursively
		if (!hierarchy || hierarchy->parent == entt::null) {
			drawEntityNode(entity, scene);
		}
	}

	// Click blank area to deselect
	if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
		_selectedEntity = entt::null;
	}

	ImGui::End();
}

void EditorLayer::drawEntityNode(entt::entity entity, Scene& scene) {
	auto& registry = scene.getRegistry();
	auto& tag = registry.get<TagComponent>(entity);
	auto* hierarchy = registry.try_get<HierarchyComponent>(entity);

	bool hasChildren = hierarchy != nullptr && hierarchy->firstChild != entt::null;

	ImGuiTreeNodeFlags flags = ((_selectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) |
							   ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	if (!hasChildren)
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", tag.name.c_str());

	if (ImGui::IsItemClicked()) {
		_selectedEntity = entity;
	}

	if (hasChildren && opened) {
		entt::entity currentChild = hierarchy->firstChild;

		while (currentChild != entt::null) {
			drawEntityNode(currentChild, scene);

			auto* childHierarchy = registry.try_get<HierarchyComponent>(currentChild);
			if (childHierarchy) {
				currentChild = childHierarchy->nextSibling;
			} else {
				FISHY_LOG_ERROR("Entity has a child without a HierarchyComponent!");
				break;
			}
		}
		ImGui::TreePop();
	}
}

// ─────────────────── Properties Panel ───────────────────

void EditorLayer::drawPropertiesPanel(Scene& scene) {
	ImGui::Begin("Properties");
	if (_selectedEntity == entt::null) {
		ImGui::TextDisabled("Select an entity to view properties.");
	} else {

		auto& registry = scene.getRegistry();

		// Handle the entity's intrinsic Tag component (name field)
		if (registry.all_of<TagComponent>(_selectedEntity)) {
			auto& tag = registry.get<TagComponent>(_selectedEntity);
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strncpy(buffer, tag.name.c_str(), sizeof(buffer) - 1);

			if (ImGui::InputText("##Tag", buffer, sizeof(buffer))) {
				tag.name = std::string(buffer);
			}
		}

		ImGui::Separator();

		// Handle the entity's Transform component allowing translation/rotation/scale editing
		if (registry.all_of<TransformComponent>(_selectedEntity)) {
			const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
													 ImGuiTreeNodeFlags_SpanAvailWidth |
													 ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;
			if (ImGui::TreeNodeEx((void*)typeid(TransformComponent).hash_code(), treeNodeFlags, "Transform")) {
				auto& transform = registry.get<TransformComponent>(_selectedEntity);

				glm::vec3 tempPos = transform.position;
				if (ImGui::DragFloat3("Position", &tempPos.x, 0.1f)) {
					transform.setPosition(tempPos);
				}

				glm::vec3 tempScale = transform.scale;
				if (ImGui::DragFloat3("Scale", &tempScale.x, 0.1f)) {
					transform.setScale(tempScale);
				}

				// Convert to degrees for more user-friendly UI editing
				glm::vec3 eulerRadians = glm::eulerAngles(transform.rotation);
				glm::vec3 eulerDegrees = glm::degrees(eulerRadians);

				if (ImGui::DragFloat3("Rotation", &eulerDegrees.x, 1.0f)) {
					transform.setRotationEuler(glm::radians(eulerDegrees));
				}
				ImGui::TreePop();
			}
		}

		ImGui::Spacing();
		
		// Utility to attach new components (temporarily stubbed out functionality)
		if (ImGui::Button("Add Component")) {
			ImGui::OpenPopup("AddComponent");
		}
		if (ImGui::BeginPopup("AddComponent")) {
			if (ImGui::MenuItem("Camera")) { /* registry.emplace<CameraComponent>(_selectedEntity); */
			}
			if (ImGui::MenuItem("Mesh Renderer")) { /* ... */
			}
			ImGui::EndPopup();
		}
	}

	ImGui::End();
}

// ─────────────────── Content Browser ───────────────────

void EditorLayer::drawContentBrowser() {
	ImGui::Begin("Content Browser");
	ImGui::Text("Assets will be displayed here");
	ImGui::End();
}

// ─────────────────── Log ───────────────────

void EditorLayer::drawLogWindow() {
	ImGui::Begin("Log");

	static bool autoScroll = true;
	ImGui::Checkbox("Auto-scroll", &autoScroll);

	ImGui::Separator();

	ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	// Fetch up to 1024 latest log items directly from the backend log system
	auto logs = LogSystem::get().get_ringbuffer_logs(1024);

	for (const auto& log_str : logs) {
		bool popColor = false;
		// Determine line color by keyword parsing. Colors are set to visually distinguish severity.
		if (log_str.find("[error]") != std::string::npos || log_str.find("[err]") != std::string::npos) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			popColor = true;
		} else if (log_str.find("[warning]") != std::string::npos || log_str.find("[warn]") != std::string::npos) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
			popColor = true;
		} else if (log_str.find("[trace]") != std::string::npos) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			popColor = true;
		} else if (log_str.find("[info]") != std::string::npos) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
			popColor = true;
		}

		ImGui::TextUnformatted(log_str.c_str());
		if (popColor) {
			ImGui::PopStyleColor();
		}
	}
	
	// Ensure the view sticks to the bottom if autoscroll is enabled
	if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();
	ImGui::End();
}

} // namespace Fishy