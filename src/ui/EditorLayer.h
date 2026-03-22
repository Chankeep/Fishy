#pragma once

#include "rendering/SceneFramebuffer.h"

#include <entt/entt.hpp>
#include <imgui.h>

namespace Fishy {

class Scene;
class VulkanDevice;

/**
 * @brief Manages the editor UI layout: DockSpace, Viewport, Scene Hierarchy.
 */
class EditorLayer {
public:
	EditorLayer() = default;
	~EditorLayer() = default;

	void init(VulkanDevice& device, vk::Format colorFormat);
	void shutdown();

	/// Call before renderToTexture() — handles offscreen resize if needed
	void beginFrame();

	void onImGui(Scene& scene);

	[[nodiscard]] SceneFramebuffer& getSceneFramebuffer() const { return *_sceneFramebuffer; }
	[[nodiscard]] ImVec2 getViewportSize() const { return _viewportSize; }
	[[nodiscard]] bool isViewportHovered() const { return _viewportHovered; }
	[[nodiscard]] bool isViewportFocused() const { return _viewportFocused; }
	[[nodiscard]] entt::entity getSelectedEntity() const { return _selectedEntity; }

private:
	void drawDockSpace();
	void drawViewport();
	void drawSceneHierarchy(Scene& scene);
	void setupDefaultLayout(ImGuiID dockspaceId);

	std::unique_ptr<SceneFramebuffer> _sceneFramebuffer;
	entt::entity _selectedEntity = entt::null;
	ImVec2 _viewportSize{1280, 720};

	bool _viewportFocused = false;
	bool _viewportHovered = false;
	bool _firstFrame = true;
};

} // namespace Fishy