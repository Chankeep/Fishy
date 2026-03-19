#pragma once

#include "../../resources/Mesh.h"
#include "math/AABB.h"
#include <cstdint>
#include <entt/entt.hpp>

namespace Fishy {

/**
 * @brief Component for storing mesh geometry data.
 *
 * This component holds an entt::resource handle to the mesh (vertices, indices).
 * It is separate from MeshRendererComponent (which holds material).
 */
struct MeshComponent {
	entt::resource<Mesh> mesh;

	Math::AABB worldAABB;
	glm::mat4 cachedWorldMatrix{0.0f};
	uint32_t meshRegionIndex = UINT32_MAX;

	MeshComponent() = default;
	explicit MeshComponent(entt::resource<Mesh> m) : mesh(std::move(m)) {}
};

} // namespace Fishy
