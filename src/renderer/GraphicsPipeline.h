#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS // 移除Vulkan.hpp的构造函数

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace Fishy {

class GraphicsPipeline {
public:
	GraphicsPipeline(vk::raii::PipelineLayout layout, vk::raii::Pipeline pipeline)
		: layout(std::move(layout)), pipeline(std::move(pipeline)) {}

	GraphicsPipeline(const GraphicsPipeline&) = delete;
	GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

	GraphicsPipeline(GraphicsPipeline&&) = default;
	GraphicsPipeline& operator=(GraphicsPipeline&&) = default;

	void bind(const vk::raii::CommandBuffer& cmd) const;

	const vk::raii::PipelineLayout& getLayout() const { return layout; }
	const vk::raii::Pipeline& getPipeline() const { return pipeline; }

private:
	// 销毁时与声明顺序相反：Pipeline 依赖 Layout (通常来说)，所以 Pipeline 先销毁，Layout 后销毁
	vk::raii::PipelineLayout layout;
	vk::raii::Pipeline pipeline;
};

} // namespace Fishy