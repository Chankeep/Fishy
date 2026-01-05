#pragma once

#include "../../resources/Mesh.h"
#include <memory>

namespace Fishy {

/**
 * @brief Component for storing mesh geometry data.
 *
 * This component holds a reference to the mesh (vertices, indices).
 * It is separate from MeshRendererComponent (which holds material).
 */
struct MeshComponent {
	std::shared_ptr<Mesh> mesh;

	MeshComponent() = default;
	explicit MeshComponent(std::shared_ptr<Mesh> m) : mesh(std::move(m)) {}
};

} // namespace Fishy
