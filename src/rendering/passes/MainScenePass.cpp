#include "MainScenePass.h"

#include "../GraphicsPipeline.h"
#include "../RenderConstants.h"

namespace Fishy {

void MainScenePass::execute(RenderGraphContext& ctx, [[maybe_unused]] entt::registry& registry) {
	// Early exit if resources are not available
	if (!ctx.vertexBuffer || !ctx.indexBuffer || ctx.drawBatches.empty()) {
		return;
	}

	// Bind pipeline (owned by this pass)
	_pipeline.bind(ctx.cmd);

	// Set dynamic state
	ctx.cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(ctx.viewportExtent.width),
										static_cast<float>(ctx.viewportExtent.height), 0.0f, 1.0f));
	ctx.cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), ctx.viewportExtent));

	// Bind global descriptor sets
	ctx.cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_pipeline.getLayout(), 0, ctx.globalDescriptorSet,
							   nullptr);
	ctx.cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_pipeline.getLayout(), 1, ctx.bindlessTextureSet,
							   nullptr);

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
}

} // namespace Fishy
