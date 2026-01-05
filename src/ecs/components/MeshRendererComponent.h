#pragma once

#include "../../rendering/Material.h"
#include <memory>

namespace Fishy {

/**
 * @brief Component for storing rendering properties (material).
 *
 * This component holds the material used to render the mesh.
 * Entities need both MeshComponent and MeshRendererComponent to be rendered.
 */
struct MeshRendererComponent {
	std::shared_ptr<Material> material;

	// Rendering flags
	bool castShadows = true;
	bool receiveShadows = true;
	bool visible = true;

	MeshRendererComponent() = default;
	explicit MeshRendererComponent(std::shared_ptr<Material> mat) : material(std::move(mat)) {}
};

} // namespace Fishy
