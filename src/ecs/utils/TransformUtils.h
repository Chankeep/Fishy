#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "../components/HierarchyComponent.h"

namespace Fishy::TransformUtils {

void setParent(entt::registry& registry, entt::entity entity, entt::entity newParent, bool worldPositionStays = true);
void setWorldPosition(entt::registry& registry, entt::entity entity, const glm::vec3& worldPos);
void markDirtyRecursive(entt::registry& registry, entt::entity entity);
void removeFromHierarchy(entt::registry& registry, entt::entity entity);

} // namespace Fishy::TransformUtils