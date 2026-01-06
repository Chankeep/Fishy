#pragma once

#include "../../rendering/Material.h"
#include <entt/entt.hpp>

namespace Fishy {

/**
 * @brief Component for storing rendering properties (material).
 *
 * This component holds an entt::resource handle to the material used to render the mesh.
 * Entities need both MeshComponent and MeshRendererComponent to be rendered.
 */
struct MeshRendererComponent {
	entt::resource<Material> material;

	// Rendering flags
	bool castShadows = true;
	bool receiveShadows = true;
	bool visible = true;

	MeshRendererComponent() = default;
	explicit MeshRendererComponent(entt::resource<Material> mat) : material(std::move(mat)) {}
};

} // namespace Fishy
