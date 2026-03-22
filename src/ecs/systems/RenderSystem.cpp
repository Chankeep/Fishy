#include "RenderSystem.h"

#include "../../rendering/Renderer.h"
#include "../../scene/Scene.h"

namespace Fishy {

void RenderSystem::render(Scene& scene, Renderer& renderer, const RenderParams& params, SceneFramebuffer& target) {
	// Pass render parameters to renderer and render scene to the offscreen target
	renderer.renderToTexture(scene, params, target);
}

} // namespace Fishy
