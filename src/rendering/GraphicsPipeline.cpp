#include "GraphicsPipeline.h"

namespace Fishy {

void GraphicsPipeline::bind(const vk::raii::CommandBuffer& cmd) const {
	cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipeline);
}

} // namespace Fishy