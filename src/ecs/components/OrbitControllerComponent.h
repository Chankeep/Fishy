#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Fishy {
struct OrbitControllerComponent {
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

    bool isDirty = true;

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


    void setTarget(const glm::vec3& newTarget) {
        target = newTarget;
        isDirty = true;
    }
    void setYawPitch(float newYaw, float newPitch) {
        yaw = newYaw;
        pitch = glm::clamp(newPitch, minPitch, maxPitch);
        isDirty = true;
    }
    void setDistance(float newDistance) {
        distance = glm::clamp(newDistance, minDistance, maxDistance);
        isDirty = true;
    }
    
    void addZoom(float scrollDelta) {
        if (scrollDelta != 0.0f) {
            distance -= scrollDelta * zoomSpeed * distance;
            distance = glm::clamp(distance, minDistance, maxDistance);
            isDirty = true;
        }
    }
};
} // namespace Fishy