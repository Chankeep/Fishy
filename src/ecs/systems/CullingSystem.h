#pragma once

#include <entt/entt.hpp>
#include <vector>

namespace Fishy {

class Scene;

/**
 * @brief System for frustum culling.
 *
 * Tests mesh AABBs against the primary camera's frustum planes.
 * Outputs a list of visible entities for the renderer to consume.
 */
class CullingSystem {
public:
	/**
	 * @brief Perform frustum culling on all mesh entities.
	 *
	 * Updates world-space AABBs when transforms change, then tests
	 * against the primary camera's frustum planes.
	 *
	 * @param scene The scene containing mesh and camera entities.
	 * @return Const reference to the visible entity list (reused buffer).
	 */
	const std::vector<entt::entity>& cull(Scene& scene);

private:
	std::vector<entt::entity> _visibleEntities;
};

} // namespace Fishy
