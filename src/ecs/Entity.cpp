#include "Entity.h"

namespace Fishy {

Entity::Entity(entt::entity handle, Scene* scene) : _handle(handle), _scene(scene) {}

} // namespace Fishy
