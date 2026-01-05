#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Fishy {

class Scene;

/**
 * @brief GPU-compatible light data structure.
 *
 * Aligned for std140 layout compatibility.
 */
struct alignas(16) LightData {
	glm::vec4 positionAndType;	 // xyz = position, w = type (0=dir, 1=point, 2=spot)
	glm::vec4 directionAndRange; // xyz = direction, w = range
	glm::vec4 colorAndIntensity; // xyz = color, w = intensity
	glm::vec4 spotAngles;		 // x = inner cone cos, y = outer cone cos, z = unused, w = unused
};

/**
 * @brief System for managing lights.
 *
 * Gathers light data from LightComponent entities and prepares it for GPU upload.
 */
class LightingSystem {
public:
	LightingSystem() = default;
	~LightingSystem() = default;

	/**
	 * @brief Update all lights in the scene.
	 *
	 * Gathers light data from entities with LightComponent and TransformComponent.
	 *
	 * @param scene The scene containing light entities.
	 */
	void update(Scene& scene);

	/**
	 * @brief Get the gathered light data for GPU upload.
	 */
	[[nodiscard]] const std::vector<LightData>& getLightData() const { return _lightData; }

	/**
	 * @brief Get the number of active lights.
	 */
	[[nodiscard]] uint32_t getLightCount() const { return static_cast<uint32_t>(_lightData.size()); }

private:
	std::vector<LightData> _lightData;
};

} // namespace Fishy
