#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <glm/glm.hpp>

namespace Fishy {

class Scene;
struct TransformComponent;

/**
 * @brief System for updating entity transforms.
 *
 * Computes world matrices from local TRS values.
 * Supports parent-child hierarchy for glTF node inheritance.
 *
 * Also provides static utility methods for common transform operations
 * that require registry access (e.g., marking children dirty).
 */
class TransformSystem {
public:
	TransformSystem() = default;
	~TransformSystem() = default;

	/**
	 * @brief Update all transforms in the scene.
	 *
	 * Iterates over all entities with TransformComponent and updates their worldMatrix.
	 *
	 * @param scene The scene to update.
	 */
	void update(Scene& scene);

	// === Static Utility Methods (require registry access) ===

	/**
	 * @brief Mark this entity and all its descendants as dirty.
	 *
	 * Call this after modifying a parent's transform to ensure all children
	 * will recalculate their world matrices on next update.
	 */
	static void markDirtyRecursive(entt::registry& registry, entt::entity entity);

	/**
	 * @brief Set world-space position for an entity.
	 *
	 * Converts world position to local space if entity has a parent.
	 * Marks the entity and all children as dirty.
	 */
	static void setWorldPosition(entt::registry& registry, entt::entity entity, const glm::vec3& worldPos);

	/**
	 * @brief Set world-space rotation for an entity.
	 *
	 * Converts world rotation to local space if entity has a parent.
	 * Marks the entity and all children as dirty.
	 */
	static void setWorldRotation(entt::registry& registry, entt::entity entity, const glm::quat& worldRot);

	/**
	 * @brief Set parent of an entity, adjusting local transform to maintain world position.
	 *
	 * @param registry The entity registry.
	 * @param child The entity to reparent.
	 * @param newParent The new parent entity (or entt::null to unparent).
	 * @param worldPositionStays If true, local transform is adjusted to keep world position.
	 */
	static void setParent(entt::registry& registry, entt::entity child, entt::entity newParent,
						  bool worldPositionStays = true);

	/**
	 * @brief Find all direct children of an entity.
	 */
	static std::vector<entt::entity> getChildren(entt::registry& registry, entt::entity parent);

	/**
	 * @brief Detach all children from an entity (set their parent to null).
	 */
	static void detachChildren(entt::registry& registry, entt::entity parent);

	/**
	 * @brief Get the root entity in the hierarchy chain.
	 */
	static entt::entity getRoot(entt::registry& registry, entt::entity entity);
};

} // namespace Fishy
