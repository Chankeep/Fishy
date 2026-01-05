#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Fishy {

/**
 * @brief Component for camera properties including orbit control.
 *
 * Entities with this component can be used as cameras in the scene.
 * The view matrix is derived from the entity's TransformComponent.
 */
struct CameraComponent {
	// Projection parameters
	float fov = 45.0f;		 // Field of view in degrees
	float nearClip = 0.1f;	 // Near clipping plane
	float farClip = 1000.0f; // Far clipping plane
	float aspectRatio = 16.0f / 9.0f;

	// Projection type
	bool orthographic = false;
	float orthoSize = 10.0f; // Half-height for orthographic projection

	// Is this the primary/active camera?
	bool primary = true;

	// Orbit camera state
	glm::vec3 target{0.0f}; // Orbit center point
	float distance = 3.0f;	// Distance from target
	float yaw = 0.0f;		// Horizontal rotation angle (degrees)
	float pitch = 0.0f;		// Vertical rotation angle (degrees)

	// Orbit control settings
	float rotateSpeed = 0.5f;
	float zoomSpeed = 0.1f;
	float panSpeed = 0.003f;
	float minDistance = 0.5f;
	float maxDistance = 50.0f;
	float minPitch = -89.0f;
	float maxPitch = 89.0f;

	// Cached matrices (updated by CameraSystem)
	glm::mat4 projectionMatrix{1.0f};
	glm::mat4 viewMatrix{1.0f};

	CameraComponent() = default;

	/**
	 * @brief Compute the projection matrix based on current settings.
	 */
	[[nodiscard]] glm::mat4 getProjectionMatrix() const {
		if (orthographic) {
			float halfWidth = orthoSize * aspectRatio;
			return glm::ortho(-halfWidth, halfWidth, -orthoSize, orthoSize, nearClip, farClip);
		}
		return glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip);
	}

	/**
	 * @brief Compute camera position from orbit parameters.
	 */
	[[nodiscard]] glm::vec3 getOrbitPosition() const {
		float yawRad = glm::radians(yaw);
		float pitchRad = glm::radians(pitch);

		glm::vec3 offset;
		offset.x = distance * cos(pitchRad) * sin(yawRad);
		offset.y = distance * sin(pitchRad);
		offset.z = distance * cos(pitchRad) * cos(yawRad);

		return target + offset;
	}
};

} // namespace Fishy
