#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Fishy {

/**
 * @brief Orbit camera for model inspection
 *
 * Controls:
 * - Right mouse button + drag: Rotate around target
 * - Mouse wheel: Zoom in/out
 * - Middle mouse button + drag: Pan
 *
 * Coordinate system: Y-up (glTF compatible)
 */
class Camera {
public:
	Camera() = default;

	/**
	 * @brief Update camera state from input
	 *
	 * Call this every frame with input state
	 *
	 * @param mouseDeltaX Mouse movement in X since last frame
	 * @param mouseDeltaY Mouse movement in Y since last frame
	 * @param scrollDelta Mouse wheel scroll delta
	 * @param mouseButtonRight Is right mouse button held
	 * @param mouseButtonMiddle Is middle mouse button held
	 * @param mouseButtonLeft Is left mouse button held
	 * @param deltaTime Frame time in seconds
	 */
	void update(float mouseDeltaX, float mouseDeltaY, float scrollDelta, bool mouseButtonRight, bool mouseButtonMiddle,
				bool mouseButtonLeft, float deltaTime);

	/**
	 * @brief Get view matrix
	 */
	[[nodiscard]] glm::mat4 getViewMatrix() const;

	/**
	 * @brief Get projection matrix
	 *
	 * @param aspect Aspect ratio (width / height)
	 * @param fov Vertical field of view in degrees
	 * @param nearPlane Near clipping plane distance
	 * @param farPlane Far clipping plane distance
	 */
	[[nodiscard]] glm::mat4 getProjectionMatrix(float aspect, float fov = 45.0f, float nearPlane = 0.1f,
												float farPlane = 100.0f) const;

	/**
	 * @brief Get camera position in world space
	 */
	[[nodiscard]] glm::vec3 getPosition() const { return _position; }

	/**
	 * @brief Get target position (orbit center) in world space
	 */
	[[nodiscard]] glm::vec3 getTarget() const { return _target; }

	/**
	 * @brief Get camera forward direction
	 */
	[[nodiscard]] glm::vec3 getForward() const;

	/**
	 * @brief Get camera up direction
	 */
	[[nodiscard]] glm::vec3 getUp() const;

	/**
	 * @brief Get camera right direction
	 */
	[[nodiscard]] glm::vec3 getRight() const;

	/**
	 * @brief Set camera target (orbit center)
	 */
	void setTarget(const glm::vec3& target) { _target = target; }

	/**
	 * @brief Set camera distance from target
	 */
	void setDistance(float distance);

	/**
	 * @brief Reset camera to default position
	 */
	void reset();

	/**
	 * @brief Set view matrix directly (for external camera control)
	 */
	void setViewMatrix(const glm::mat4& view) {
		_overrideViewMatrix = view;
		_useOverrideMatrices = true;
	}

	/**
	 * @brief Set projection matrix directly (for external camera control)
	 */
	void setProjectionMatrix(const glm::mat4& proj) {
		_overrideProjectionMatrix = proj;
		_useOverrideMatrices = true;
	}

	/**
	 * @brief Set position directly (for external camera control)
	 */
	void setPosition(const glm::vec3& pos) { _position = pos; }

	/**
	 * @brief Clear override matrices and return to orbit mode
	 */
	void clearOverrideMatrices() { _useOverrideMatrices = false; }

private:
	void updatePosition();
	void rotate(float deltaX, float deltaY);
	void zoom(float delta);
	void pan(float deltaX, float deltaY);

	// Camera parameters
	glm::vec3 _target = glm::vec3(0.0f); // Orbit center point
	float _distance = 3.0f;				 // Distance from target
	float _yaw = 0.0f;					 // Horizontal rotation angle
	float _pitch = 0.0f;				 // Vertical rotation angle

	// Derived position (updated from angles and distance)
	glm::vec3 _position = glm::vec3(0.0f, 0.0f, 3.0f);

	// Control settings
	float _rotateSpeed = 0.5f;	// Rotation sensitivity
	float _zoomSpeed = 0.1f;	// Zoom sensitivity
	float _panSpeed = 0.003f;	// Pan sensitivity (slower for smoother control)
	float _minDistance = 0.5f;	// Minimum zoom distance
	float _maxDistance = 50.0f; // Maximum zoom distance

	// Pitch limits to prevent gimbal lock
	float _minPitch = -89.0f; // Minimum pitch angle
	float _maxPitch = 89.0f;  // Maximum pitch angle

	// Override matrices for external camera control
	bool _useOverrideMatrices = false;
	glm::mat4 _overrideViewMatrix{1.0f};
	glm::mat4 _overrideProjectionMatrix{1.0f};
};

} // namespace Fishy
