#include "TransformSystem.h"

#include "../../scene/Scene.h"
#include "../components/TransformComponent.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace Fishy {

// Helper function to recursively update world matrices
static void updateEntityWorldMatrix(entt::registry& registry, entt::entity entity, const glm::mat4& parentWorldMatrix) {
	auto& transform = registry.get<TransformComponent>(entity);

	// Compute world matrix: parent * local
	transform.worldMatrix = parentWorldMatrix * transform.getLocalMatrix();
	transform.dirty = false;

	// Find and update all children that have this entity as parent
	auto view = registry.view<TransformComponent>();
	for (auto child : view) {
		auto& childTransform = view.get<TransformComponent>(child);
		if (childTransform.parent == entity) {
			// Always update children when parent updates
			updateEntityWorldMatrix(registry, child, transform.worldMatrix);
		}
	}
}

// Helper to propagate dirty flag upward to root
static void propagateDirtyToRoot(entt::registry& registry, entt::entity entity) {
	while (entity != entt::null) {
		auto* transform = registry.try_get<TransformComponent>(entity);
		if (!transform)
			break;

		if (transform->dirty) {
			// Already dirty, ancestors should already be marked
			break;
		}
		transform->dirty = true;
		entity = transform->parent;
	}
}

void TransformSystem::update(Scene& scene) {
	auto& registry = scene.getRegistry();
	auto view = registry.view<TransformComponent>();

	// Pass 1: Find all dirty nodes and propagate dirty flag up to their root
	for (auto entity : view) {
		auto& transform = view.get<TransformComponent>(entity);
		if (transform.dirty && transform.parent != entt::null) {
			// Propagate dirty upward
			propagateDirtyToRoot(registry, transform.parent);
		}
	}

	// Pass 2: Update from all dirty root entities downward
	for (auto entity : view) {
		auto& transform = view.get<TransformComponent>(entity);
		if (transform.parent == entt::null && transform.dirty) {
			// Root entity with dirty flag: update entire subtree
			updateEntityWorldMatrix(registry, entity, glm::mat4(1.0f));
		}
	}
}

// === Static Utility Methods ===

void TransformSystem::markDirtyRecursive(entt::registry& registry, entt::entity entity) {
	if (!registry.valid(entity))
		return;

	auto* transform = registry.try_get<TransformComponent>(entity);
	if (!transform)
		return;

	transform->dirty = true;

	// Mark all children as dirty
	auto view = registry.view<TransformComponent>();
	for (auto child : view) {
		auto& childTransform = view.get<TransformComponent>(child);
		if (childTransform.parent == entity) {
			markDirtyRecursive(registry, child);
		}
	}
}

void TransformSystem::setWorldPosition(entt::registry& registry, entt::entity entity, const glm::vec3& worldPos) {
	if (!registry.valid(entity))
		return;

	auto* transform = registry.try_get<TransformComponent>(entity);
	if (!transform)
		return;

	if (transform->parent == entt::null) {
		// Root entity: world position = local position
		transform->position = worldPos;
	} else {
		// Has parent: convert world to local
		auto* parentTransform = registry.try_get<TransformComponent>(transform->parent);
		if (parentTransform) {
			// Local = inverse(parentWorld) * worldPos
			glm::mat4 invParent = glm::inverse(parentTransform->worldMatrix);
			transform->position = glm::vec3(invParent * glm::vec4(worldPos, 1.0f));
		}
	}

	markDirtyRecursive(registry, entity);
}

void TransformSystem::setWorldRotation(entt::registry& registry, entt::entity entity, const glm::quat& worldRot) {
	if (!registry.valid(entity))
		return;

	auto* transform = registry.try_get<TransformComponent>(entity);
	if (!transform)
		return;

	if (transform->parent == entt::null) {
		// Root entity: world rotation = local rotation
		transform->rotation = worldRot;
	} else {
		// Has parent: convert world to local
		auto* parentTransform = registry.try_get<TransformComponent>(transform->parent);
		if (parentTransform) {
			// Extract parent world rotation
			glm::vec3 parentScale, parentTranslation, skew;
			glm::vec4 perspective;
			glm::quat parentWorldRot;
			glm::decompose(parentTransform->worldMatrix, parentScale, parentWorldRot, parentTranslation, skew,
						   perspective);

			// Local = inverse(parentWorldRot) * worldRot
			transform->rotation = glm::inverse(parentWorldRot) * worldRot;
		}
	}

	markDirtyRecursive(registry, entity);
}

void TransformSystem::setParent(entt::registry& registry, entt::entity child, entt::entity newParent,
								bool worldPositionStays) {
	if (!registry.valid(child))
		return;

	auto* childTransform = registry.try_get<TransformComponent>(child);
	if (!childTransform)
		return;

	// Prevent circular hierarchy
	if (newParent != entt::null) {
		auto current = newParent;
		while (current != entt::null) {
			if (current == child) {
				// Would create circular reference
				return;
			}
			auto* parentTrans = registry.try_get<TransformComponent>(current);
			current = parentTrans ? parentTrans->parent : entt::null;
		}
	}

	if (worldPositionStays) {
		// Remember current world transform
		glm::vec3 worldPos = childTransform->getWorldPosition();

		// Set new parent
		childTransform->parent = newParent;

		// Adjust local transform to maintain world position
		setWorldPosition(registry, child, worldPos);
	} else {
		// Just change parent, local transform stays the same
		childTransform->parent = newParent;
		markDirtyRecursive(registry, child);
	}
}

std::vector<entt::entity> TransformSystem::getChildren(entt::registry& registry, entt::entity parent) {
	std::vector<entt::entity> children;

	auto view = registry.view<TransformComponent>();
	for (auto entity : view) {
		auto& transform = view.get<TransformComponent>(entity);
		if (transform.parent == parent) {
			children.push_back(entity);
		}
	}

	return children;
}

void TransformSystem::detachChildren(entt::registry& registry, entt::entity parent) {
	auto children = getChildren(registry, parent);
	for (auto child : children) {
		setParent(registry, child, entt::null, true);
	}
}

entt::entity TransformSystem::getRoot(entt::registry& registry, entt::entity entity) {
	if (!registry.valid(entity))
		return entt::null;

	auto* transform = registry.try_get<TransformComponent>(entity);
	if (!transform)
		return entity;

	while (transform->parent != entt::null) {
		entity = transform->parent;
		transform = registry.try_get<TransformComponent>(entity);
		if (!transform)
			break;
	}

	return entity;
}

} // namespace Fishy
