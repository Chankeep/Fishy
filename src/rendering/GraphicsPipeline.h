#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace Fishy {

class GraphicsPipeline {
public:
	GraphicsPipeline(vk::raii::PipelineLayout layout, vk::raii::Pipeline pipeline)
		: _layout(std::move(layout)), _pipeline(std::move(pipeline)) {}

	GraphicsPipeline(const GraphicsPipeline&) = delete;
	GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
	GraphicsPipeline(GraphicsPipeline&&) = default;
	GraphicsPipeline& operator=(GraphicsPipeline&&) = default;

	void bind(const vk::raii::CommandBuffer& cmd) const;

	[[nodiscard]] const vk::raii::PipelineLayout& getLayout() const { return _layout; }
	[[nodiscard]] const vk::raii::Pipeline& getPipeline() const { return _pipeline; }

private:
	// Destruction order is reverse of declaration: Pipeline depends on Layout (usually), so Pipeline destroyed first,
	// Layout last
	vk::raii::PipelineLayout _layout;
	vk::raii::Pipeline _pipeline;
};

} // namespace Fishy