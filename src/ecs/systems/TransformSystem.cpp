#include "TransformSystem.h"

#include "../../scene/Scene.h"
#include "../components/TransformComponent.h"

namespace Fishy {

void TransformSystem::update(Scene& scene) {
	auto view = scene.view<TransformComponent>();

	for (auto entity : view) {
		auto& transform = view.get<TransformComponent>(entity);

		if (transform.dirty) {
			// Compute world matrix from local TRS
			// For now, no hierarchy - worldMatrix = localMatrix
			transform.worldMatrix = transform.getLocalMatrix();
			transform.dirty = false;
		}
	}
}

} // namespace Fishy
