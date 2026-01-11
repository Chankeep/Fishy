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
	glm::vec3 position{0.0f, 0.0f, 0.0f};
	glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // Identity quaternion
	glm::vec3 scale{1.0f, 1.0f, 1.0f};

	// Parent entity for hierarchical transforms (entt::null if root)
	entt::entity parent{entt::null};

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

	// === Getters ===

	[[nodiscard]] const glm::vec3& getPosition() const { return position; }
	[[nodiscard]] const glm::quat& getRotation() const { return rotation; }
	[[nodiscard]] const glm::vec3& getScale() const { return scale; }
	[[nodiscard]] glm::vec3 getRotationEuler() const { return glm::eulerAngles(rotation); }

	// === World Space Getters (from cached worldMatrix) ===

	[[nodiscard]] glm::vec3 getWorldPosition() const { return glm::vec3(worldMatrix[3]); }

	[[nodiscard]] glm::vec3 getForward() const { return glm::normalize(glm::vec3(worldMatrix[2])); }

	[[nodiscard]] glm::vec3 getRight() const { return glm::normalize(glm::vec3(worldMatrix[0])); }

	[[nodiscard]] glm::vec3 getUp() const { return glm::normalize(glm::vec3(worldMatrix[1])); }

	// === Hierarchy Helpers ===

	[[nodiscard]] bool hasParent() const { return parent != entt::null; }
	[[nodiscard]] bool isRoot() const { return parent == entt::null; }
};

} // namespace Fishy
