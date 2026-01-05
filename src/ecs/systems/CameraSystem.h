#pragma once

#include <glm/glm.hpp>

namespace Fishy {

class Scene;

/**
 * @brief System for managing cameras.
 *
 * Updates camera view/projection matrices and handles aspect ratio changes.
 * Also processes mouse input for orbit camera control.
 */
class CameraSystem {
public:
	CameraSystem() = default;
	~CameraSystem() = default;

	/**
	 * @brief Update all cameras in the scene.
	 *
	 * @param scene The scene containing camera entities.
	 * @param aspectRatio Current viewport aspect ratio.
	 */
	void update(Scene& scene, float aspectRatio);

	/**
	 * @brief Process mouse input for orbit camera control.
	 *
	 * Updates primary camera's orbit state based on input.
	 *
	 * @param scene The scene containing camera entities.
	 * @param mouseDeltaX Mouse X movement since last frame.
	 * @param mouseDeltaY Mouse Y movement since last frame.
	 * @param scrollDelta Mouse wheel scroll delta.
	 * @param rightButton Is right mouse button held.
	 * @param middleButton Is middle mouse button held.
	 */
	void processInput(Scene& scene, float mouseDeltaX, float mouseDeltaY, float scrollDelta, bool rightButton,
					  bool middleButton);

	/**
	 * @brief Get the view matrix of the primary camera.
	 */
	[[nodiscard]] glm::mat4 getPrimaryViewMatrix() const { return _primaryViewMatrix; }

	/**
	 * @brief Get the projection matrix of the primary camera.
	 */
	[[nodiscard]] glm::mat4 getPrimaryProjectionMatrix() const { return _primaryProjectionMatrix; }

	/**
	 * @brief Get the position of the primary camera.
	 */
	[[nodiscard]] glm::vec3 getPrimaryCameraPosition() const { return _primaryCameraPosition; }

private:
	glm::mat4 _primaryViewMatrix{1.0f};
	glm::mat4 _primaryProjectionMatrix{1.0f};
	glm::vec3 _primaryCameraPosition{0.0f};
};

} // namespace Fishy
