#pragma once

#include "entt/entity/fwd.hpp"
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <glm/glm.hpp>

namespace Fishy {

class Scene;
struct TransformComponent;

/**
 * @brief System for updating entity transforms.
 *
 * Computes world matrices from local TRS values.
 * Supports parent-child hierarchy for glTF node inheritance.
 *
 * Also provides static utility methods for common transform operations
 * that require registry access (e.g., marking children dirty).
 */
class TransformSystem {
public:
	TransformSystem() = default;
	~TransformSystem() = default;


	/**
	 * @brief Update all transforms in the scene.
	 *
	 * Iterates over all entities with TransformComponent and updates their worldMatrix.
	 *
	 * @param scene The scene to update.
	 */
	void update(Scene& scene);

};

} // namespace Fishy
