#include "AABB.h"

#include <cmath>

namespace Fishy::Math {

void AABB::expand(const glm::vec3& point) {
	min = glm::min(min, point);
	max = glm::max(max, point);
}

glm::vec3 AABB::center() const {
	return (min + max) * 0.5f;
}

glm::vec3 AABB::extents() const {
	return (max - min) * 0.5f;
}

// Arvo's fast AABB transform: transform center normally, then compute
// new extents by multiplying abs(matrix_3x3) * old_extents.
// This gives a conservative (slightly oversized) but correct AABB.
AABB AABB::transformed(const glm::mat4& matrix) const {
	glm::vec3 c = center();
	glm::vec3 e = extents();

	// Transform center as a point (w=1)
	glm::vec3 newCenter = glm::vec3(matrix * glm::vec4(c, 1.0f));

	// New extents: sum of abs(row_i) * extents per axis
	// GLM is column-major: matrix[col][row]
	glm::vec3 newExtents{0.0f};
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			newExtents[i] += std::abs(matrix[j][i]) * e[j];
		}
	}

	AABB result;
	result.min = newCenter - newExtents;
	result.max = newCenter + newExtents;
	return result;
}

// Test AABB against 6 frustum planes using the p-vertex method.
// For each plane, find the AABB corner most aligned with the normal (p-vertex).
// If the p-vertex is behind the plane, the entire AABB is outside.
bool AABB::isOnFrustum(const std::array<glm::vec4, 6>& planes) const {
	for (const auto& plane : planes) {
		// Build p-vertex: pick max component when normal is positive, min otherwise
		glm::vec3 pVertex{
			(plane.x >= 0.0f) ? max.x : min.x,
			(plane.y >= 0.0f) ? max.y : min.y,
			(plane.z >= 0.0f) ? max.z : min.z,
		};

		// Signed distance: N dot pVertex + d
		float distance = glm::dot(glm::vec3(plane), pVertex) + plane.w;

		if (distance < 0.0f) {
			return false;
		}
	}

	return true;
}

} // namespace Fishy::Math