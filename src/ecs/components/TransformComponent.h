#pragma once

#include <entt/entity/entity.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Fishy {

/**
 * @brief Component for storing an entity's transform (position, rotation, scale).
 *
 * Uses TRS (Translation, Rotation, Scale) representation.
 * Supports parent-child hierarchy for glTF node inheritance.
 * Provides helper to compute the final model matrix.
 *
 * @note For operations that affect children, use TransformSystem utility methods.
 */
struct TransformComponent {
	glm::vec3 position{0.0f};
	glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // Identity quaternion
	glm::vec3 scale{1.0f};

	// Cached world matrix (updated by TransformSystem)
	glm::mat4 worldMatrix{1.0f};
	glm::mat4 localMatrix{1.0f};

	// Flag to indicate if the local transform has changed
	bool dirty = true;

	const glm::mat4&  getMatrix() const {return worldMatrix;}
	const glm::mat4&  getLocalMatrix() const {return localMatrix;}


	// === Local Transform Setters (marks dirty) ===

	void setPosition(const glm::vec3& pos) {
		position = pos;
		dirty = true;
	}

	void setRotation(const glm::quat& rot) {
		rotation = rot;
		dirty = true;
	}

	void setScale(const glm::vec3& scl) {
		scale = scl;
		dirty = true;
	}

	void setRotationEuler(const glm::vec3& eulerRadians) {
		rotation = glm::quat(eulerRadians);
		dirty = true;
	}

	// === Transform Manipulation ===

	void translate(const glm::vec3& delta) {
		position += delta;
		dirty = true;
	}

	void rotate(const glm::quat& deltaRot) {
		rotation = deltaRot * rotation;
		dirty = true;
	}

	void rotateEuler(const glm::vec3& eulerRadians) {
		rotation = glm::quat(eulerRadians) * rotation;
		dirty = true;
	}

	void scaleBy(const glm::vec3& factor) {
		scale *= factor;
		dirty = true;
	}

	void scaleUniform(float factor) {
		scale *= factor;
		dirty = true;
	}
};

} // namespace Fishy
