#include "TransformSystem.h"

#include "../../scene/Scene.h"
#include "../components/HierarchyComponent.h"
#include "../components/TransformComponent.h"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>

namespace Fishy {

void updateTransformTree(entt::registry& registry, entt::entity entity, const glm::mat4& parentMatrix,
						 bool parentDirty) {
	auto* transform = registry.try_get<TransformComponent>(entity);
	bool forceUpdateChildren = parentDirty;

	if (transform) {
		if (transform->dirty || parentDirty) {
			if (transform->dirty) {
				transform->localMatrix = glm::translate(glm::mat4(1.0f), transform->position) *
										 glm::mat4_cast(transform->rotation) *
										 glm::scale(glm::mat4(1.0f), transform->scale);
			}
			transform->worldMatrix = parentMatrix * transform->localMatrix;
			transform->dirty = false;
			forceUpdateChildren = true;
		}
	}

	auto* hierarchy = registry.try_get<HierarchyComponent>(entity);
	if (hierarchy) {
		entt::entity child = hierarchy->firstChild;
		while (child != entt::null) {
			const glm::mat4& passMatrix = transform ? transform->worldMatrix : parentMatrix;
			updateTransformTree(registry, child, passMatrix, forceUpdateChildren);

			child = registry.get<HierarchyComponent>(child).nextSibling;
		}
	}
}

void TransformSystem::update(Scene& scene) {
	auto& registry = scene.getRegistry();

	auto view = registry.view<TransformComponent>();
	for (auto entity : view) {
		auto* hierarchy = registry.try_get<HierarchyComponent>(entity);
		if (!hierarchy || hierarchy->parent == entt::null) {
			updateTransformTree(registry, entity, glm::mat4(1.0f), false);
		}
	}
}

} // namespace Fishy
