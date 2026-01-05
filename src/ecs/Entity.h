#pragma once

#include <entt/entt.hpp>

namespace Fishy {

class Scene;

/**
 * @brief Lightweight wrapper around entt::entity for convenient component access.
 *
 * This class provides a friendlier API for adding, getting, and checking components
 * on an entity without directly dealing with the registry.
 */
class Entity {
public:
	Entity() = default;
	Entity(entt::entity handle, Scene* scene);
	Entity(const Entity&) = default;
	Entity& operator=(const Entity&) = default;

	/**
	 * @brief Add a component to this entity.
	 * @tparam T Component type.
	 * @tparam Args Constructor argument types.
	 * @param args Arguments forwarded to component constructor.
	 * @return Reference to the newly added component.
	 */
	template <typename T, typename... Args> T& addComponent(Args&&... args);

	/**
	 * @brief Get a component from this entity.
	 * @tparam T Component type.
	 * @return Reference to the component.
	 */
	template <typename T> [[nodiscard]] T& getComponent();

	template <typename T> [[nodiscard]] const T& getComponent() const;

	/**
	 * @brief Check if this entity has a component.
	 * @tparam T Component type.
	 * @return True if the entity has the component.
	 */
	template <typename T> [[nodiscard]] bool hasComponent() const;

	/**
	 * @brief Remove a component from this entity.
	 * @tparam T Component type.
	 */
	template <typename T> void removeComponent();

	/**
	 * @brief Get the underlying entt::entity handle.
	 */
	[[nodiscard]] entt::entity getHandle() const { return _handle; }

	/**
	 * @brief Get the parent scene.
	 */
	[[nodiscard]] Scene* getScene() const { return _scene; }

	/**
	 * @brief Check if this entity is valid.
	 */
	[[nodiscard]] bool isValid() const { return _handle != entt::null && _scene != nullptr; }

	/**
	 * @brief Implicit conversion to bool for validity check.
	 */
	operator bool() const { return isValid(); }

	/**
	 * @brief Comparison operators.
	 */
	bool operator==(const Entity& other) const { return _handle == other._handle && _scene == other._scene; }
	bool operator!=(const Entity& other) const { return !(*this == other); }

private:
	entt::entity _handle{entt::null};
	Scene* _scene = nullptr;
};

} // namespace Fishy

// Include template implementations
#include "Entity.inl"
