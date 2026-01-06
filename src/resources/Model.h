#pragma once

#include "../rendering/Material.h"
#include "Mesh.h"
#include <entt/entt.hpp>
#include <vector>

namespace Fishy {

/**
 * @brief A single primitive within a Model.
 *
 * Uses entt::resource handles for mesh and material, integrating with the
 * ResourceManager's caching system.
 */
struct Primitive {
	entt::resource<Mesh> mesh;
	entt::resource<Material> material;
};

/**
 * @brief A loaded 3D model containing multiple primitives.
 *
 * Each primitive consists of a mesh and a material. This is typically
 * loaded from glTF files via ModelLoader.
 */
class Model {
public:
	Model() = default;
	~Model() = default;

	void addPrimitive(const Primitive& primitive) { _primitives.push_back(primitive); }
	void addPrimitive(Primitive&& primitive) { _primitives.push_back(std::move(primitive)); }
	[[nodiscard]] const std::vector<Primitive>& getPrimitives() const { return _primitives; }
	[[nodiscard]] size_t getPrimitiveCount() const { return _primitives.size(); }
	[[nodiscard]] bool empty() const { return _primitives.empty(); }

private:
	std::vector<Primitive> _primitives;
};

} // namespace Fishy
