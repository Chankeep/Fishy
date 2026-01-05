#include "Scene.h"

#include "Entity.h"
#include "components/TagComponent.h"
#include "components/TransformComponent.h"

namespace Fishy {

Entity Scene::createEntity(const std::string& name) {
	Entity entity{_registry.create(), this};
	entity.addComponent<TagComponent>(name);
	entity.addComponent<TransformComponent>();
	return entity;
}

void Scene::destroyEntity(Entity entity) { _registry.destroy(entity.getHandle()); }

} // namespace Fishy
