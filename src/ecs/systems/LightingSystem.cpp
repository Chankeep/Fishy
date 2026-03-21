#include "LightingSystem.h"

#include "../../scene/Scene.h"
#include "../components/LightComponent.h"
#include "../components/TransformComponent.h"
#include <cmath>

namespace Fishy {

void LightingSystem::update(Scene& scene) {
	_lightData.clear();

	auto view = scene.view<LightComponent, TransformComponent>();

	for (auto entity : view) {
		const auto& light = view.get<LightComponent>(entity);
		const auto& transform = view.get<TransformComponent>(entity);

		LightData data;

		// Position and type
		data.positionAndType = glm::vec4(transform.position, static_cast<float>(light.type));

		// Direction (forward vector from rotation) and range
		// For directional lights, direction is -Z in local space transformed by rotation
		glm::vec3 forward = glm::mat3(transform.worldMatrix) * glm::vec3(0.0f, 0.0f, -1.0f);
		data.directionAndRange = glm::vec4(glm::normalize(forward), light.range);

		// Color and intensity
		data.colorAndIntensity = glm::vec4(light.color, light.intensity);

		// Spot angles + shadow info
		// z = shadowIndex placeholder (0 = eligible, Renderer patches actual index; -1 = no shadow)
		data.spotAngles =
			glm::vec4(std::cos(glm::radians(light.innerConeAngle)), std::cos(glm::radians(light.outerConeAngle)),
					  light.castShadows ? 0.0f : -1.0f, 0.0f);

		_lightData.push_back(data);
	}
}

} // namespace Fishy
