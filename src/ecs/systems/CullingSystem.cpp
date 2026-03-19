#include "CullingSystem.h"

#include "../../scene/Scene.h"
#include "../components/CameraComponent.h"
#include "../components/MeshComponent.h"
#include "../components/MeshRendererComponent.h"
#include "../components/TransformComponent.h"

namespace Fishy {

const std::vector<entt::entity>& CullingSystem::cull(Scene& scene) {
	_visibleEntities.clear();

	auto& registry = scene.getRegistry();

	// Find primary camera frustum planes
	const std::array<glm::vec4, 6>* frustumPlanes = nullptr;
	{
		auto camView = registry.view<CameraComponent>();
		for (auto entity : camView) {
			const auto& cam = camView.get<CameraComponent>(entity);
			if (cam.primary) {
				frustumPlanes = &cam.frustumPlanes;
				break;
			}
		}
	}

	// Iterate all renderable entities
	auto view = registry.view<MeshComponent, MeshRendererComponent, TransformComponent>();
	for (auto entity : view) {
		auto& meshComp = view.get<MeshComponent>(entity);
		const auto& meshRenderer = view.get<MeshRendererComponent>(entity);
		const auto& transform = view.get<TransformComponent>(entity);

		if (!meshComp.mesh || !meshRenderer.visible) {
			continue;
		}

		// Update world AABB if transform changed
		if (meshComp.cachedWorldMatrix != transform.worldMatrix) {
			meshComp.cachedWorldMatrix = transform.worldMatrix;
			meshComp.worldAABB = meshComp.mesh->getLocalAABB().transformed(transform.worldMatrix);
		}

		// Skip frustum test if no camera (show everything)
		if (!frustumPlanes || meshComp.worldAABB.isOnFrustum(*frustumPlanes)) {
			_visibleEntities.push_back(entity);
		}
	}

	return _visibleEntities;
}

} // namespace Fishy
