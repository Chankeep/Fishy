#include "Mesh.h"

namespace Fishy {

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
	: _vertices(vertices), _indices(indices) {}

} // namespace Fishy
