#include "ShadowMapPass.h"

#include "../GraphicsPipeline.h"
#include "../RenderConstants.h"
#include "core/VulkanUtils.h"
#include "vulkan/vulkan.hpp"

namespace Fishy {

void ShadowMapPass::execute(RenderGraphContext& ctx, [[maybe_unused]] entt::registry& registry) {
	// Early exit if nothing to render
	if (!ctx.vertexBuffer || !ctx.indexBuffer || ctx.drawBatches.empty() || ctx.shadowCasterCount == 0) {
		return;
	}

	// Transition atlas to depth attachment
	VulkanUtils::transitionImage(ctx.cmd, _shadowMap->getImage(), vk::ImageLayout::eUndefined,
								 vk::ImageLayout::eDepthAttachmentOptimal, vk::AccessFlagBits2::eNone,
								 vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
								 vk::PipelineStageFlagBits2::eTopOfPipe,
								 vk::PipelineStageFlagBits2::eEarlyFragmentTests, vk::ImageAspectFlagBits::eDepth);

	// Begin rendering — clear entire atlas once
	vk::ClearValue clearDepth{.depthStencil = {1.0f, 0}};

	vk::RenderingAttachmentInfo depthAttachment{.imageView = _shadowMap->getImageView(),
												.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
												.loadOp = vk::AttachmentLoadOp::eClear,
												.storeOp = vk::AttachmentStoreOp::eStore,
												.clearValue = clearDepth};

	vk::RenderingInfo renderingInfo{.renderArea = vk::Rect2D{{0, 0}, {SHADOW_ATLAS_SIZE, SHADOW_ATLAS_SIZE}},
									.layerCount = 1,
									.colorAttachmentCount = 0,
									.pColorAttachments = nullptr,
									.pDepthAttachment = &depthAttachment};

	ctx.cmd.beginRendering(renderingInfo);

	// Bind pipeline and geometry once
	_pipeline.bind(ctx.cmd);
	ctx.cmd.bindVertexBuffers(0, ctx.vertexBuffer->getBuffer(), {0});
	ctx.cmd.bindIndexBuffer(ctx.indexBuffer->getBuffer(), 0, vk::IndexType::eUint32);

	// Render each shadow caster into its own atlas tile
	for (uint32_t i = 0; i < ctx.shadowCasterCount; ++i) {
		auto [tileX, tileY] = shadowTilePixelOffset(i);

		// Set viewport/scissor to this tile's region
		ctx.cmd.setViewport(0, vk::Viewport(static_cast<float>(tileX), static_cast<float>(tileY),
											static_cast<float>(SHADOW_TILE_SIZE), static_cast<float>(SHADOW_TILE_SIZE),
											0.0f, 1.0f));
		ctx.cmd.setScissor(0, vk::Rect2D(vk::Offset2D(tileX, tileY), {SHADOW_TILE_SIZE, SHADOW_TILE_SIZE}));

		// Push constants with current shadow tile index
		PushConstants pc{.instanceDataAddress = ctx.instanceDataAddress,
						 .globalDataAddress = ctx.globalDataAddress,
						 .lightDataAddress = ctx.lightDataAddress,
						 .shadowDataAddress = ctx.shadowDataAddress,
						 .shadowRenderIndex = i};

		ctx.cmd.pushConstants<PushConstants>(*_pipeline.getLayout(),
											vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
											pc);

		// Draw all scene geometry for this light's shadow
		for (const auto& batch : ctx.drawBatches) {
			ctx.cmd.drawIndexedIndirect(ctx.indirectBuffer->getBuffer(),
										batch.firstCommand * sizeof(vk::DrawIndexedIndirectCommand),
										batch.commandCount, sizeof(vk::DrawIndexedIndirectCommand));
		}
	}

	ctx.cmd.endRendering();

	// Transition atlas for shader sampling
	VulkanUtils::transitionImage(
		ctx.cmd, _shadowMap->getImage(), vk::ImageLayout::eDepthAttachmentOptimal,
		vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		vk::AccessFlagBits2::eShaderSampledRead, vk::PipelineStageFlagBits2::eLateFragmentTests,
		vk::PipelineStageFlagBits2::eFragmentShader, vk::ImageAspectFlagBits::eDepth);
}

} // namespace Fishy
