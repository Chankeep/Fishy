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

		// 1. Check & Update Projection Matrix
		camera.setAspectRatio(aspectRatio);

		if (camera.isProjectionDirty) {
			camera.projectionMatrix = camera.getProjectionMatrix();
			camera.projectionMatrix[1][1] *= -1; // Vulkan Y-axis is inverted
			camera.isProjectionDirty = false;
		}

		// 2. Check & Update View Matrix
		const auto& currentWorldMat = transform.getMatrix();
		if (camera.cachedTransformMatrix != currentWorldMat) {
			camera.cachedTransformMatrix = currentWorldMat;
			camera.markViewDirty();
		}

		if (camera.isViewDirty) {
			// Using inverse of world matrix for view matrix is efficient and general.
			// It respects hierarchy rotations, translations.
			camera.viewMatrix = glm::inverse(currentWorldMat);
			camera.isViewDirty = false;
		}
	}
}

std::optional<std::tuple<glm::mat4, glm::mat4, glm::vec3>> CameraSystem::getPrimaryCameraData(Scene& scene) const {
	auto view = scene.view<CameraComponent, TransformComponent>();

	for (auto entity : view) {
		const auto& camera = view.get<CameraComponent>(entity);
		const auto& transform = view.get<TransformComponent>(entity);

		if (camera.primary) {
			return std::make_tuple(camera.viewMatrix, camera.projectionMatrix, transform.position);
		}
	}

	return std::nullopt;
}

} // namespace Fishy
