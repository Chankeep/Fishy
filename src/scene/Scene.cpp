#include "Scene.h"

#include "../ecs/Entity.h"
#include "../ecs/components/TagComponent.h"
#include "../ecs/components/TransformComponent.h"

namespace Fishy {

Entity Scene::createEntity(const std::string& name) {
	Entity entity{_registry.create(), this};
	entity.addComponent<TagComponent>(name);
	entity.addComponent<TransformComponent>();
	return entity;
}

void Scene::destroyEntity(Entity entity) { _registry.destroy(entity.getHandle()); }

std::vector<Entity> Scene::getAllEntities() {
	std::vector<Entity> entities;
	// storage<entt::entity>() returns a reference for non-const registry
	auto& storage = _registry.storage<entt::entity>();
	entities.reserve(storage.in_use());
	for (auto entity : storage) {
		entities.emplace_back(entity, this);
	}
	return entities;
}

} // namespace Fishy
