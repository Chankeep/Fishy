#pragma once

namespace Fishy {

class Scene;

/**
 * @brief System for managing orbit camera control.
 *
 * Processes mouse input to adjust the OrbitControllerComponent,
 * and updates the associated TransformComponent based on orbit state.
 */
class OrbitControllerSystem {
public:
	OrbitControllerSystem() = default;
	~OrbitControllerSystem() = default;

	/**
	 * @brief Update external Transform based on OrbitController state.
	 *
	 * @param scene The scene containing orbit entities.
	 */
	void update(Scene& scene);

	/**
	 * @brief Process mouse input for orbit camera control.
	 *
	 * Updates the first active/primary orbit controller's state based on input.
	 *
	 * @param scene The scene.
	 * @param mouseDeltaX Mouse X movement since last frame.
	 * @param mouseDeltaY Mouse Y movement since last frame.
	 * @param scrollDelta Mouse wheel scroll delta.
	 * @param rightButton Is right mouse button held.
	 * @param middleButton Is middle mouse button held.
	 */
	void processInput(Scene& scene, float mouseDeltaX, float mouseDeltaY, float scrollDelta, bool rightButton,
					  bool middleButton);
};

} // namespace Fishy
