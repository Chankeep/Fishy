#include "CameraSystem.h"

#include "../../scene/Scene.h"
#include "../components/CameraComponent.h"
#include "../components/TransformComponent.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Fishy {

void CameraSystem::update(Scene& scene, float aspectRatio) {
	auto view = scene.view<CameraComponent, TransformComponent>();

	for (auto entity : view) {
		auto& camera = view.get<CameraComponent>(entity);
		auto& transform = view.get<TransformComponent>(entity);

		// Update aspect ratio
		camera.aspectRatio = aspectRatio;

		// Update position from orbit parameters
		transform.position = camera.getOrbitPosition();

		// Compute rotation to look at target
		glm::vec3 forward = glm::normalize(camera.target - transform.position);
		glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
		glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
		glm::vec3 up = glm::cross(right, forward);

		// Compute projection matrix (with Vulkan Y-flip)
		camera.projectionMatrix = camera.getProjectionMatrix();
		camera.projectionMatrix[1][1] *= -1; // Vulkan Y-axis is inverted

		// Compute view matrix (lookAt)
		camera.viewMatrix = glm::lookAt(transform.position, camera.target, up);

		// If this is the primary camera, cache its matrices
		if (camera.primary) {
			_primaryViewMatrix = camera.viewMatrix;
			_primaryProjectionMatrix = camera.projectionMatrix;
			_primaryCameraPosition = transform.position;
		}
	}
}

void CameraSystem::processInput(Scene& scene, float mouseDeltaX, float mouseDeltaY, float scrollDelta, bool rightButton,
								bool middleButton) {
	auto view = scene.view<CameraComponent, TransformComponent>();

	for (auto entity : view) {
		auto& camera = view.get<CameraComponent>(entity);
		auto& transform = view.get<TransformComponent>(entity);

		// Only process primary camera
		if (!camera.primary) {
			continue;
		}

		// Rotate (right mouse button)
		if (rightButton) {
			camera.yaw -= mouseDeltaX * camera.rotateSpeed;
			camera.pitch += mouseDeltaY * camera.rotateSpeed;

			// Clamp pitch to avoid gimbal lock
			camera.pitch = glm::clamp(camera.pitch, camera.minPitch, camera.maxPitch);
		}

		// Pan (middle mouse button)
		if (middleButton) {
			// Compute camera right and up vectors for panning
			glm::vec3 forward = glm::normalize(camera.target - transform.position);
			glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
			glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
			glm::vec3 up = glm::cross(right, forward);

			// Pan the target (camera follows due to orbit)
			float panX = -mouseDeltaX * camera.panSpeed * camera.distance;
			float panY = mouseDeltaY * camera.panSpeed * camera.distance;
			camera.target += right * panX;
			camera.target += up * panY;
		}

		// Zoom (scroll wheel)
		if (scrollDelta != 0.0f) {
			camera.distance -= scrollDelta * camera.zoomSpeed * camera.distance;
			camera.distance = glm::clamp(camera.distance, camera.minDistance, camera.maxDistance);
		}

		break; // Only process first primary camera
	}
}

} // namespace Fishy
