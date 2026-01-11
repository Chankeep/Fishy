#include "ShadowMapPass.h"

#include "../GraphicsPipeline.h"
#include "../RenderConstants.h"
#include "core/VulkanUtils.h"
#include "vulkan/vulkan.hpp"

namespace Fishy {

void ShadowMapPass::execute(RenderGraphContext& ctx, [[maybe_unused]] entt::registry& registry) {
	// Early exit if resources are not available
	if (!ctx.vertexBuffer || !ctx.indexBuffer || ctx.drawBatches.empty()) {
		return;
	}

	// === Shadow pass has its own render pass ===
	VulkanUtils::transitionImage(ctx.cmd, _shadowMapImage->getImage(), vk::ImageLayout::eUndefined,
								 vk::ImageLayout::eDepthAttachmentOptimal, vk::AccessFlagBits2::eNone,
								 vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
								 vk::PipelineStageFlagBits2::eTopOfPipe,
								 vk::PipelineStageFlagBits2::eEarlyFragmentTests, vk::ImageAspectFlagBits::eDepth);

	vk::ClearValue clearDepth{.depthStencil = {1.0f, 0}};

	vk::RenderingAttachmentInfo depthAttachment{.imageView = *_shadowMapImage->getView(),
												.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
												.loadOp = vk::AttachmentLoadOp::eClear,
												.storeOp = vk::AttachmentStoreOp::eStore,
												.clearValue = clearDepth};

	vk::RenderingInfo renderingInfo{.renderArea = vk::Rect2D{{0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}},
									.layerCount = 1,
									.colorAttachmentCount = 0,
									.pColorAttachments = nullptr,
									.pDepthAttachment = &depthAttachment};

	ctx.cmd.beginRendering(renderingInfo);

	// Bind pipeline (owned by this pass)
	_pipeline.bind(ctx.cmd);

	// Set dynamic state for shadow map size
	ctx.cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(SHADOW_MAP_SIZE),
										static_cast<float>(SHADOW_MAP_SIZE), 0.0f, 1.0f));
	ctx.cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}));

	// Push constants for BDA
	PushConstants pc{.instanceDataAddress = ctx.instanceDataAddress, .globalDataAddress = ctx.globalDataAddress};

	ctx.cmd.pushConstants<PushConstants>(*_pipeline.getLayout(),
										 vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, pc);

	// Bind unified geometry buffers ONCE for all draw calls
	ctx.cmd.bindVertexBuffers(0, ctx.vertexBuffer->getBuffer(), {0});
	ctx.cmd.bindIndexBuffer(ctx.indexBuffer->getBuffer(), 0, vk::IndexType::eUint32);

	// Render using indirect draw calls
	for (const auto& batch : ctx.drawBatches) {
		ctx.cmd.drawIndexedIndirect(ctx.indirectBuffer->getBuffer(),
									batch.firstCommand * sizeof(vk::DrawIndexedIndirectCommand), batch.commandCount,
									sizeof(vk::DrawIndexedIndirectCommand));
	}

	ctx.cmd.endRendering();

	VulkanUtils::transitionImage(
		ctx.cmd, _shadowMapImage->getImage(), vk::ImageLayout::eDepthAttachmentOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::AccessFlagBits2::eShaderSampledRead, vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::PipelineStageFlagBits2::eFragmentShader, vk::ImageAspectFlagBits::eDepth);
}

} // namespace Fishy
