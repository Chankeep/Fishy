#pragma once

#include <entt/entt.hpp>
#include <functional>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace Fishy {

class Scene;
class Renderer;
struct LightData;

/**
 * @brief Parameters for rendering a frame.
 *
 * Contains camera and light data gathered from ECS systems.
 */
struct RenderParams {
	// Camera data (from CameraSystem)
	glm::mat4 viewMatrix{1.0f};
	glm::mat4 projectionMatrix{1.0f};
	glm::vec3 cameraPosition{0.0f, 0.0f, 3.0f};

	// Light data (from LightingSystem, mutable for shadow index patching)
	std::vector<LightData>* lights = nullptr;

	// Visible entities after frustum culling (from CullingSystem)
	const std::vector<entt::entity>* visibleEntities = nullptr;
};

/**
 * @brief System for rendering entities.
 *
 * Acts as the bridge between ECS and the low-level Renderer.
 * Iterates entities with MeshComponent, MeshRendererComponent, TransformComponent
 * and submits them to the Renderer for drawing.
 */
class RenderSystem {
public:
	RenderSystem() = default;
	~RenderSystem() = default;

	/**
	 * @brief Render the scene.
	 *
	 * @param scene The scene containing entities to render.
	 * @param renderer The Vulkan renderer backend.
	 * @param params Camera and light data for this frame.
	 * @param uiCallback Optional callback for UI rendering.
	 */
	void render(Scene& scene, Renderer& renderer, const RenderParams& params,
				std::function<void(VkCommandBuffer)> uiCallback = nullptr);
};

} // namespace Fishy
