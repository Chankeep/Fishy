#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Fishy {

/**
 * @brief Component for storing an entity's transform (position, rotation, scale).
 *
 * Uses TRS (Translation, Rotation, Scale) representation.
 * Provides helper to compute the final model matrix.
 */
struct TransformComponent {
	glm::vec3 position{0.0f, 0.0f, 0.0f};
	glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // Identity quaternion
	glm::vec3 scale{1.0f, 1.0f, 1.0f};

	// Cached world matrix (updated by TransformSystem)
	glm::mat4 worldMatrix{1.0f};

	// Flag to indicate if the local transform has changed
	bool dirty = true;

	TransformComponent() = default;
	TransformComponent(const glm::vec3& pos) : position(pos), dirty(true) {}
	TransformComponent(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scl)
		: position(pos), rotation(rot), scale(scl), dirty(true) {}

	/**
	 * @brief Compute the local transform matrix from position, rotation, and scale.
	 * @return 4x4 transformation matrix.
	 */
	[[nodiscard]] glm::mat4 getLocalMatrix() const {
		glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), position);
		glm::mat4 rotationMat = glm::mat4_cast(rotation);
		glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
		return translationMat * rotationMat * scaleMat;
	}

	/**
	 * @brief Set rotation from Euler angles (in radians).
	 */
	void setRotationEuler(const glm::vec3& eulerRadians) {
		rotation = glm::quat(eulerRadians);
		dirty = true;
	}

	/**
	 * @brief Get rotation as Euler angles (in radians).
	 */
	[[nodiscard]] glm::vec3 getRotationEuler() const { return glm::eulerAngles(rotation); }
};

} // namespace Fishy
