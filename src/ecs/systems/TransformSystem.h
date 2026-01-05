#pragma once

namespace Fishy {

class Scene;

/**
 * @brief System for updating entity transforms.
 *
 * Computes world matrices from local TRS values.
 * In the future, this can handle parent-child hierarchy.
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
