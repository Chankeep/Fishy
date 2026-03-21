#include "SkyboxPass.h"

#include "../GraphicsPipeline.h"
#include "../RenderConstants.h"

namespace Fishy {

void SkyboxPass::execute(RenderGraphContext& ctx, [[maybe_unused]] entt::registry& registry) {
	// Early exit if resources are not available
	if (!_vertexBuffer || !_indexBuffer || !ctx.iblEnvironment) {
		return;
	}

	// === Begin skybox render pass (load previous content) ===
	vk::RenderingAttachmentInfo colorAttachment{.imageView = ctx.colorAttachmentView,
												.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
												.loadOp = vk::AttachmentLoadOp::eLoad, // Keep MainRenderPass output
												.storeOp = vk::AttachmentStoreOp::eStore};

	vk::RenderingAttachmentInfo depthAttachment{.imageView = ctx.depthAttachmentView,
												.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
												.loadOp = vk::AttachmentLoadOp::eLoad, // Keep depth from MainRenderPass
												.storeOp = vk::AttachmentStoreOp::eDontCare};

	vk::RenderingInfo renderingInfo{.renderArea = vk::Rect2D{{0, 0}, ctx.viewportExtent},
									.layerCount = 1,
									.colorAttachmentCount = 1,
									.pColorAttachments = &colorAttachment,
									.pDepthAttachment = &depthAttachment};

	ctx.cmd.beginRendering(renderingInfo);

	// Bind skybox pipeline (owned by this pass)
	_pipeline.bind(ctx.cmd);

	// Set dynamic state
	ctx.cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(ctx.viewportExtent.width),
										static_cast<float>(ctx.viewportExtent.height), 0.0f, 1.0f));
	ctx.cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), ctx.viewportExtent));

	// Bind IBL descriptor set (Set 0)
	ctx.cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_pipeline.getLayout(), 0, ctx.globalDescriptorSet,
							   nullptr);

	// Bind skybox geometry (owned by this pass)
	ctx.cmd.bindVertexBuffers(0, _vertexBuffer->getBuffer(), {0});
	ctx.cmd.bindIndexBuffer(_indexBuffer->getBuffer(), 0, vk::IndexType::eUint32);

	// Push constants (BDA pointers — skybox only uses globalDataAddress)
	PushConstants pc{.instanceDataAddress = 0,
					 .globalDataAddress = ctx.globalDataAddress,
					 .lightDataAddress = 0,
					 .shadowDataAddress = 0};

	ctx.cmd.pushConstants<PushConstants>(*_pipeline.getLayout(),
										 vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pc);

										 
	// Draw skybox
	ctx.cmd.drawIndexed(_indexCount, 1, 0, 0, 0);

	ctx.cmd.endRendering();
}

} // namespace Fishy
