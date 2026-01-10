#include "UIPass.h"

namespace Fishy {

void UIPass::execute(RenderGraphContext& ctx, [[maybe_unused]] entt::registry& registry) {
	// Skip if no callback or no image view
	if (!_callback || !_currentImageView) {
		return;
	}

	// UIPass uses its own separate rendering pass with different settings:
	// - Load existing color (preserve scene rendering)
	// - No depth attachment

	vk::RenderingAttachmentInfo uiColorAttachment{.imageView = _currentImageView,
												  .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
												  .loadOp = vk::AttachmentLoadOp::eLoad,
												  .storeOp = vk::AttachmentStoreOp::eStore};

	vk::RenderingInfo uiRenderingInfo{.renderArea = vk::Rect2D{{0, 0}, ctx.viewportExtent},
									  .layerCount = 1,
									  .colorAttachmentCount = 1,
									  .pColorAttachments = &uiColorAttachment};

	ctx.cmd.beginRendering(uiRenderingInfo);

	// Execute UI callback (ImGui rendering)
	_callback(static_cast<VkCommandBuffer>(*ctx.cmd));

	ctx.cmd.endRendering();
}

} // namespace Fishy
