#pragma once

#include <entt/entt.hpp>

namespace Fishy{

    struct HierarchyComponent{
        entt::entity parent{entt::null};
        entt::entity firstChild{entt::null};
        entt::entity prevSibling{entt::null};
        entt::entity nextSibling{entt::null};
    };
}