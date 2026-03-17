#pragma once

#include <array>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Fishy {

enum class RenderTargetType { Screen, Texture };

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
	RenderTargetType renderTarget = RenderTargetType::Screen;

	// Cached matrices (updated by CameraSystem)
	glm::mat4 projectionMatrix{1.0f};
	glm::mat4 viewMatrix{1.0f};
	glm::mat4 cachedTransformMatrix{0.0f};

	bool isProjectionDirty = true;
	bool isViewDirty = true;

	std::array<glm::vec4, 6> frustumPlanes;
	bool isFrustumDirty = true;

	CameraComponent() = default;

	void setFov(float newFov) {
		fov = newFov;
		isProjectionDirty = true;
	}

	void setNearClip(float newNear) {
		nearClip = newNear;
		isProjectionDirty = true;
	}

	void setFarClip(float newFar) {
		farClip = newFar;
		isProjectionDirty = true;
	}

	void setAspectRatio(float newRatio) {
		if (std::abs(aspectRatio - newRatio) > 1e-5f) {
			aspectRatio = newRatio;
			isProjectionDirty = true;
		}
	}

	void setOrthographic(bool isOrtho, float size = 10.0f) {
		orthographic = isOrtho;
		orthoSize = size;
		isProjectionDirty = true;
	}

	void markViewDirty() { isViewDirty = true; }

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
};

} // namespace Fishy
