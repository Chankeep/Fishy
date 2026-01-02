#include "Mesh.h"

namespace Fishy {

Mesh::Mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
	: _vertices(std::move(vertices)), _indices(std::move(indices)) {}

} // namespace Fishy
