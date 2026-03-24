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

	// Initialize the EditorLayer, creating the scene framebuffer for rendering
	void init(VulkanDevice& device, vk::Format colorFormat);

	// Clean up resources used by the EditorLayer
	void shutdown();

	/// Call before renderToTexture() — handles offscreen resize if needed
	/// Ensures the scene framebuffer dimensions match the viewport size before rendering
	void beginFrame();

	// Draw the entire ImGui interface for the editor
	void onImGui(Scene& scene);

	[[nodiscard]] SceneFramebuffer& getSceneFramebuffer() const { return *_sceneFramebuffer; }
	[[nodiscard]] ImVec2 getViewportSize() const { return _viewportSize; }
	[[nodiscard]] bool isViewportHovered() const { return _viewportHovered; }
	[[nodiscard]] bool isViewportFocused() const { return _viewportFocused; }
	[[nodiscard]] entt::entity getSelectedEntity() const { return _selectedEntity; }

private:
	// Draw the main DockSpace for arranging windows
	void drawDockSpace();
	// Draw the primary 3D viewport containing the rendered scene
	void drawViewport();
	// Draw the log window displaying console messages
	void drawLogWindow();
	// Draw the content browser for assets
	void drawContentBrowser();
	// Draw the tree view representing the current scene hierarchy
	void drawSceneHierarchy(Scene& scene);
	// Recursively draw a single entity node and its children in the local hierarchy
	void drawEntityNode(entt::entity entity, Scene& scene);
	// Draw the properties panel for inspecting/editing the selected entity
	void drawPropertiesPanel(Scene& scene);
	// Construct the initial default window layout
	void setupDefaultLayout(ImGuiID dockspaceId);

	std::unique_ptr<SceneFramebuffer> _sceneFramebuffer;
	entt::entity _selectedEntity = entt::null;
	ImVec2 _viewportSize{1280, 720};

	bool _viewportFocused = false;
	bool _viewportHovered = false;
	bool _firstFrame = true;
};

} // namespace Fishy