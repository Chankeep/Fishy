#pragma once

#include <array>
#include <glm/glm.hpp>

namespace Fishy::Math {

struct AABB {
	glm::vec3 min{std::numeric_limits<float>::max()};
	glm::vec3 max{std::numeric_limits<float>::lowest()};

	void expand(const glm::vec3& point);
	[[nodiscard]] glm::vec3 center() const;
	[[nodiscard]] glm::vec3 extents() const;

	// Transform object-space AABB to world-space using Arvo's method
	[[nodiscard]] AABB transformed(const glm::mat4& matrix) const;

	// Test visibility against 6 frustum planes (p-vertex method)
	[[nodiscard]] bool isOnFrustum(const std::array<glm::vec4, 6>& planes) const;
};

} // namespace Fishy::Math
