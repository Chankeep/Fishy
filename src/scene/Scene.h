#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <string>

namespace Fishy {

class Entity;

/**
 * @brief Scene class holding all entities via EnTT registry.
 *
 * The Scene is the central container for all game objects (entities).
 * It wraps entt::registry and provides a simplified API for entity management.
 */
class Scene {
public:
	Scene() = default;
	~Scene() = default;

	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&&) = default;
	Scene& operator=(Scene&&) = default;

	/**
	 * @brief Create a new entity with an optional name.
	 * @param name Optional name for the entity (used in TagComponent).
	 * @return Entity wrapper for the newly created entity.
	 */
	Entity createEntity(const std::string& name = "Entity");

	/**
	 * @brief Destroy an entity and all its components.
	 * @param entity The entity to destroy.
	 */
	void destroyEntity(Entity entity);

	/**
	 * @brief Get direct access to the underlying registry.
	 * @return Reference to the entt::registry.
	 */
	[[nodiscard]] entt::registry& getRegistry() { return _registry; }
	[[nodiscard]] const entt::registry& getRegistry() const { return _registry; }

	/**
	 * @brief Get a view of entities with specific components.
	 * @tparam Components Component types to filter by.
	 * @return EnTT view for iteration.
	 */
	template <typename... Components> [[nodiscard]] auto view() { return _registry.view<Components...>(); }

	template <typename... Components> [[nodiscard]] auto view() const { return _registry.view<Components...>(); }

private:
	entt::registry _registry;
};

} // namespace Fishy
