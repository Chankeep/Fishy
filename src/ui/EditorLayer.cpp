#include "EditorLayer.h"

#include "ecs/components/TagComponent.h"
#include "scene/Scene.h"

#include <imgui.h>
#include <imgui_internal.h> // for DockBuilder API

namespace Fishy {
void EditorLayer::init(VulkanDevice& device, vk::Format colorFormat) {
	_sceneFramebuffer = std::make_unique<SceneFramebuffer>(device, colorFormat);
	FISHY_LOG_INFO("EditorLayer initialized");
}
void EditorLayer::shutdown() {
	_sceneFramebuffer.reset();
	FISHY_LOG_INFO("EditorLayer shut down");
}

void EditorLayer::beginFrame() {
	auto w = static_cast<uint32_t>(_viewportSize.x);
	auto h = static_cast<uint32_t>(_viewportSize.y);
	if (w > 0 && h > 0 && _sceneFramebuffer->needsResize(w, h)) {
		_sceneFramebuffer->resize(w, h);
	}
}
void EditorLayer::onImGui(Scene& scene) {
	drawDockSpace();
	drawViewport();
	drawSceneHierarchy(scene);
}

// ─────────────────── DockSpace ───────────────────

void EditorLayer::drawDockSpace() {
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
							 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavFocus |
							 ImGuiWindowFlags_NoBringToFrontOnFocus;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("DockSpace", nullptr, flags);
	ImGui::PopStyleVar(3);

	ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");

	if (_firstFrame) {
		setupDefaultLayout(dockspaceId);
		_firstFrame = false;
	}

	ImGui::DockSpace(dockspaceId, ImVec2(0, 0), ImGuiDockNodeFlags_None);
	ImGui::End();
}
void EditorLayer::setupDefaultLayout(ImGuiID dockspaceId) {
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

	ImGuiID leftId, rightId;
	ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.2f, &leftId, &rightId);

	ImGuiID leftTopId, leftBottomId;
	ImGui::DockBuilderSplitNode(leftId, ImGuiDir_Up, 0.5f, &leftTopId, &leftBottomId);

	ImGui::DockBuilderDockWindow("Scene Hierarchy", leftTopId);
	ImGui::DockBuilderDockWindow("Shader Debug", leftBottomId);
	ImGui::DockBuilderDockWindow("Viewport", rightId);
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

	for (auto entity : view) {
		auto& tag = view.get<TagComponent>(entity);

		bool isSelected = (_selectedEntity == entity);

		if (ImGui::Selectable(tag.name.c_str(), isSelected)) {
			_selectedEntity = entity;
		}
	}

	// Click blank area to deselect
	if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
		_selectedEntity = entt::null;
	}

	ImGui::End();
}
} // namespace Fishy