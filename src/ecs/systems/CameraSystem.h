#pragma once

#include <glm/glm.hpp>
#include <optional>
#include <tuple>

namespace Fishy {

class Scene;

/**
 * @brief System for managing cameras.
 *
 * Updates camera view/projection matrices and handles aspect ratio changes.
 */
class CameraSystem {
public:
	CameraSystem() = default;
	~CameraSystem() = default;

	/**
	 * @brief Update all cameras in the scene based on dirty state.
	 *
	 * @param scene The scene containing camera entities.
	 * @param aspectRatio Current viewport aspect ratio.
	 */
	void update(Scene& scene, float aspectRatio);

	/**
	 * @brief Helper to retrieve the primary camera's matrices and position.
	 * 
	 * @param scene The scene.
	 * @return A tuple of [ViewMatrix, ProjectionMatrix, CameraPosition] if a primary camera is found.
	 */
	std::optional<std::tuple<glm::mat4, glm::mat4, glm::vec3>> getPrimaryCameraData(Scene& scene) const;
};

} // namespace Fishy
