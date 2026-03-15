#include "TransformUtils.h"

#include "../components/TransformComponent.h"
#include "entt/entity/fwd.hpp"

namespace Fishy::TransformUtils {

void removeFromHierarchy(entt::registry& registry, entt::entity entity) {
	auto* hierarchy = registry.try_get<HierarchyComponent>(entity);
	if (!hierarchy || hierarchy->parent == entt::null) return;

	auto& parentHierarchy = registry.get<HierarchyComponent>(hierarchy->parent);

	if (hierarchy->prevSibling != entt::null) {
		registry.get<HierarchyComponent>(hierarchy->prevSibling).nextSibling = hierarchy->nextSibling;
	} else {
		parentHierarchy.firstChild = hierarchy->nextSibling;
	}

	if (hierarchy->nextSibling != entt::null) {
		registry.get<HierarchyComponent>(hierarchy->nextSibling).prevSibling = hierarchy->prevSibling;
	}

	hierarchy->parent = entt::null;
	hierarchy->prevSibling = entt::null;
	hierarchy->nextSibling = entt::null;
}

void setParent(entt::registry& registry, entt::entity entity, entt::entity newParent,
			   bool worldPositionStays) {

	// Remember current world position before detaching
	glm::vec3 worldPos(0.0f);
	auto* transform = registry.try_get<TransformComponent>(entity);
	if (worldPositionStays && transform) {
		worldPos = glm::vec3(transform->worldMatrix[3]);
	}

	// Detach from current tree
	removeFromHierarchy(registry, entity);

	if (newParent != entt::null) {
		auto& childHierarchy = registry.get_or_emplace<HierarchyComponent>(entity);
		auto& parentHierarchy = registry.get_or_emplace<HierarchyComponent>(newParent);

		childHierarchy.parent = newParent;
		childHierarchy.nextSibling = parentHierarchy.firstChild;
		childHierarchy.prevSibling = entt::null;

		if (parentHierarchy.firstChild != entt::null) {
			registry.get<HierarchyComponent>(parentHierarchy.firstChild).prevSibling = entity;
		}
		parentHierarchy.firstChild = entity;
	}

	// Restore world position to compute correct local transform in new parent space
	if (worldPositionStays && transform) {
		setWorldPosition(registry, entity, worldPos);
	} else {
		markDirtyRecursive(registry, entity);
	}
}

void setWorldPosition(entt::registry& registry, entt::entity entity, const glm::vec3& worldPos) {
	auto* transform = registry.try_get<TransformComponent>(entity);

	if (!transform)
		return;

	auto* hierarchy = registry.try_get<HierarchyComponent>(entity);
	if (!hierarchy || hierarchy->parent == entt::null) {
		transform->position = worldPos;
	} else {
		// Has parent: convert world to local
		auto* parentTransform = registry.try_get<TransformComponent>(hierarchy->parent);
		if (parentTransform) {
			// Local = inverse(parentWorld) * worldPos
			glm::mat4 invParent = glm::inverse(parentTransform->worldMatrix);
			transform->position = glm::vec3(invParent * glm::vec4(worldPos, 1.0f));
		}
	}

	markDirtyRecursive(registry, entity);
}

void markDirtyRecursive(entt::registry& registry, entt::entity entity) {
	auto* transform = registry.try_get<TransformComponent>(entity);

	if (transform) {
		transform->dirty = true;
	}

	// Mark all children as dirty
	auto* hierarchy = registry.try_get<HierarchyComponent>(entity);
	if (hierarchy) {
		entt::entity child = hierarchy->firstChild;
		while (child != entt::null) {
			markDirtyRecursive(registry, child);
			child = registry.get<HierarchyComponent>(child).nextSibling;
		}
	}
}

} // namespace Fishy::TransformUtils