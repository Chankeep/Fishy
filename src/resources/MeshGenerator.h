#pragma once

#include "Mesh.h" // Reuse existing Vertex structure

#include <memory>

namespace Fishy {

/**
 * @brief Tool class for generating basic geometric meshes.
 */
class MeshGenerator {
public:
	/**
	 * @brief Generate a unit cube mesh (side length 2, center at origin).
	 * @return A Mesh object containing vertex and index data.
	 *
	 * Used for Skybox rendering: vertex positions also act as direction vectors for sampling the Cubemap.
	 */
	[[nodiscard]] static std::shared_ptr<Mesh> createCube();
};

} // namespace Fishy