#pragma once

#include "../renderer/Material.h"
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
	const std::vector<Primitive>& getPrimitives() const { return _primitives; }

private:
	std::vector<Primitive> _primitives;
};

} // namespace Fishy
