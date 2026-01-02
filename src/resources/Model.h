#pragma once

#include "../rendering/Material.h"
#include "Mesh.h"
#include <memory>
#include <vector>

namespace Fishy {

struct Primitive {
	std::shared_ptr<Mesh> mesh;
	std::shared_ptr<Material> material;
};

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
