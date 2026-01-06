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

} // namespace Fishy
