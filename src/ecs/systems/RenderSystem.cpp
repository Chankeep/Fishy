#include "RenderSystem.h"

#include "../../rendering/Renderer.h"
#include "../../scene/Scene.h"

namespace Fishy {

void RenderSystem::render(Scene& scene, Renderer& renderer, const RenderParams& params,
						  std::function<void(VkCommandBuffer)> uiCallback) {
	// Pass render parameters to renderer and render scene
	renderer.renderScene(scene, params, uiCallback);
}

} // namespace Fishy
