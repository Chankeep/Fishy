#include "OrbitControllerSystem.h"

#include "../../scene/Scene.h"
#include "../components/OrbitControllerComponent.h"
#include "../components/CameraComponent.h" // Needed potentially if we only update primary? Let's assume we update all dirty controllers
#include "../components/TransformComponent.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Fishy {

void OrbitControllerSystem::update(Scene& scene) {
	auto view = scene.view<OrbitControllerComponent, TransformComponent>();

	for (auto entity : view) {
		auto& orbit = view.get<OrbitControllerComponent>(entity);
		auto& transform = view.get<TransformComponent>(entity);

		if (orbit.isDirty) {
			// Update position from orbit parameters
			glm::vec3 newPosition = orbit.getOrbitPosition();
			transform.setPosition(newPosition);

			// Compute rotation to look at target
			glm::vec3 forward = glm::normalize(orbit.target - newPosition);
			
			// Compute rotation quat. Assume up is always Y+ for simple orbit.
			// Using glm::lookAt gives a View matrix. To get world rotation quaternion from forward direction:
			glm::mat4 viewMat = glm::lookAt(newPosition, orbit.target, glm::vec3(0.0f, 1.0f, 0.0f));
			// View matrix transforms world to camera. We want camera to world, so inverse.
			glm::mat4 cameraWorld = glm::inverse(viewMat);
			glm::quat newRot = glm::quat_cast(cameraWorld);

			transform.setRotation(newRot);

			orbit.isDirty = false;
		}
	}
}

void OrbitControllerSystem::processInput(Scene& scene, float mouseDeltaX, float mouseDeltaY, float scrollDelta,
										 bool rightButton, bool middleButton) {
	// Typically we want to process the primary camera only. 
	// The View contains CameraComponent to check primary flag, and OrbitControllerComponent.
	auto view = scene.view<OrbitControllerComponent, CameraComponent, TransformComponent>();

	for (auto entity : view) {
		auto& orbit = view.get<OrbitControllerComponent>(entity);
		auto& camera = view.get<CameraComponent>(entity);

		if (!camera.primary) {
			continue;
		}

		// Rotate (right mouse button)
		if (rightButton) {
			orbit.setYawPitch(orbit.yaw - mouseDeltaX * orbit.rotateSpeed,
							  orbit.pitch + mouseDeltaY * orbit.rotateSpeed);
		}

		// Pan (middle mouse button)
		if (middleButton) {
			// Need transform position to compute correct panning plane
			auto& transform = view.get<TransformComponent>(entity);
			
			// Instead of recalculating, we can use the entity's current rotation if available.
			// But since we want to pan parallel to the screen:
			glm::vec3 forward = glm::normalize(orbit.target - transform.position);
			glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
			glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
			glm::vec3 up = glm::cross(right, forward);

			float panX = -mouseDeltaX * orbit.panSpeed * orbit.distance;
			float panY = mouseDeltaY * orbit.panSpeed * orbit.distance;

			orbit.setTarget(orbit.target + right * panX + up * panY);
		}

		// Zoom (scroll wheel)
		if (scrollDelta != 0.0f) {
			orbit.addZoom(scrollDelta);
		}

		break; // Only process first primary camera's orbit controller
	}
}

} // namespace Fishy
