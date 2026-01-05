#pragma once

#include <glm/glm.hpp>

namespace Fishy {

/**
 * @brief Light type enumeration.
 */
enum class LightType { Directional, Point, Spot };

/**
 * @brief Component for light sources.
 *
 * Entities with this component act as lights in the scene.
 * Position/direction is derived from the entity's TransformComponent.
 */
struct LightComponent {
	LightType type = LightType::Directional;
	glm::vec3 color{1.0f, 1.0f, 1.0f};
	float intensity = 1.0f;

	// Point/Spot light attenuation
	float range = 10.0f;

	// Spot light cone angles (in degrees)
	float innerConeAngle = 30.0f;
	float outerConeAngle = 45.0f;

	// Shadow casting
	bool castShadows = true;

	LightComponent() = default;
	LightComponent(LightType t, const glm::vec3& c, float i) : type(t), color(c), intensity(i) {}
};

} // namespace Fishy
