#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include "GraphicsPipeline.h"

namespace Fishy {

class PipelineBuilder {
public:
	explicit PipelineBuilder(vk::Device device);

	void clear();

	PipelineBuilder& setShaders(const vk::raii::ShaderModule& vertShader, const vk::raii::ShaderModule& fragShader);
	PipelineBuilder& setShaders(const vk::raii::ShaderModule& vertShader, const vk::raii::ShaderModule& fragShader,
								const char* vertEntry, const char* fragEntry);
	PipelineBuilder& setInputTopology(vk::PrimitiveTopology topology);
	PipelineBuilder& setPolygonMode(vk::PolygonMode mode);
	PipelineBuilder& setCullMode(vk::CullModeFlags cullMode, vk::FrontFace frontFace);
	PipelineBuilder& setDepthStencilTest(bool depthWriteEnable, bool depthTestEnable, vk::CompareOp compareOp,
										 bool stencilTestEnable, vk::CompareOp stencilCompareOp);
	PipelineBuilder& enableAlphaBlending();
	PipelineBuilder& setLayout(const std::vector<vk::DescriptorSetLayout>& layouts,
							   const std::vector<vk::PushConstantRange>& pushConstants);
	PipelineBuilder& setVertexInput(const vk::PipelineVertexInputStateCreateInfo& info);
	PipelineBuilder& setRenderingFormats(const std::vector<vk::Format>& colorFormats, vk::Format depthFormat);
	std::unique_ptr<GraphicsPipeline> build(const vk::raii::Device& device, vk::RenderPass renderPass = nullptr);

private:
	std::vector<vk::PipelineShaderStageCreateInfo> _shaderStages;
	std::vector<vk::DescriptorSetLayout> _descriptorSetLayouts;
	std::vector<vk::PushConstantRange> _pushConstantRanges;
	std::vector<vk::DynamicState> _dynamicStates;
	std::vector<vk::Format> _colorAttachmentFormats;
	vk::Format _depthAttachmentFormat;

	vk::PipelineInputAssemblyStateCreateInfo _inputAssembly;
	vk::PipelineRasterizationStateCreateInfo _rasterizer;
	vk::PipelineMultisampleStateCreateInfo _multisampling;
	vk::PipelineDepthStencilStateCreateInfo _depthStencil;
	vk::PipelineColorBlendAttachmentState _colorBlendAttachment;
	vk::PipelineVertexInputStateCreateInfo _vertexInputInfo;
};
} // namespace Fishy