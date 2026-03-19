#include "Frustum.h"

namespace Fishy::Math {

void extractFrustumPlanes(const glm::mat4& viewProj, std::array<glm::vec4, 6>& planes, bool normalizePlanes,
						  bool vulkanDepthRange) {
	// The Frustum plane equation is Ax + By + Cz + D = 0.
	// When normal points INSIDE:
	// Left:   row3 + row0
	// Right:  row3 - row0
	// Bottom: row3 + row1
	// Top:    row3 - row1
	// Near:   row2 (for [0,1] depth) or row3 + row2 (for [-1,1] depth)
	// Far:    row3 - row2

	planes[static_cast<size_t>(FrustumPlaneIndex::Left)] =
		glm::vec4(viewProj[0][3] + viewProj[0][0], viewProj[1][3] + viewProj[1][0],
				  viewProj[2][3] + viewProj[2][0], viewProj[3][3] + viewProj[3][0]);

	planes[static_cast<size_t>(FrustumPlaneIndex::Right)] =
		glm::vec4(viewProj[0][3] - viewProj[0][0], viewProj[1][3] - viewProj[1][0],
				  viewProj[2][3] - viewProj[2][0], viewProj[3][3] - viewProj[3][0]);

	planes[static_cast<size_t>(FrustumPlaneIndex::Top)] =
		glm::vec4(viewProj[0][3] - viewProj[0][1], viewProj[1][3] - viewProj[1][1],
				  viewProj[2][3] - viewProj[2][1], viewProj[3][3] - viewProj[3][1]);

	planes[static_cast<size_t>(FrustumPlaneIndex::Buttom)] =
		glm::vec4(viewProj[0][3] + viewProj[0][1], viewProj[1][3] + viewProj[1][1],
				  viewProj[2][3] + viewProj[2][1], viewProj[3][3] + viewProj[3][1]);

	if (vulkanDepthRange) {
		planes[static_cast<size_t>(FrustumPlaneIndex::Near)] =
			glm::vec4(viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2]);
	} else {
		planes[static_cast<size_t>(FrustumPlaneIndex::Near)] =
			glm::vec4(viewProj[0][3] + viewProj[0][2], viewProj[1][3] + viewProj[1][2],
					  viewProj[2][3] + viewProj[2][2], viewProj[3][3] + viewProj[3][2]);
	}

	planes[static_cast<size_t>(FrustumPlaneIndex::Far)] =
		glm::vec4(viewProj[0][3] - viewProj[0][2], viewProj[1][3] - viewProj[1][2],
				  viewProj[2][3] - viewProj[2][2], viewProj[3][3] - viewProj[3][2]);

	if (normalizePlanes) {
		for (auto& plane : planes) {
			float length = glm::length(glm::vec3(plane.x, plane.y, plane.z));
			if (length > 0.0f) {
				plane /= length;
			}

			// FISHY_LOG_TRACE("Frustum plane: {}, {}, {}, {}", plane.x, plane.y, plane.z, plane.w);
		}
	}
}

} // namespace Fishy::Math
