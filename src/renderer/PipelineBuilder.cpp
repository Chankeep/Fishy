#include "PipelineBuilder.h"

namespace Fishy {

PipelineBuilder::PipelineBuilder(vk::Device /*device*/) { clear(); }

void PipelineBuilder::clear() {
	_shaderStages.clear();
	_inputAssembly = vk::PipelineInputAssemblyStateCreateInfo{{}, vk::PrimitiveTopology::eTriangleList, VK_FALSE};
	_rasterizer = vk::PipelineRasterizationStateCreateInfo{{},
														   VK_FALSE,
														   VK_FALSE,
														   vk::PolygonMode::eFill,
														   vk::CullModeFlagBits::eBack,
														   vk::FrontFace::eCounterClockwise,
														   VK_FALSE,
														   0.0f,
														   0.0f,
														   0.0f,
														   1.0f};
	_multisampling = vk::PipelineMultisampleStateCreateInfo{
		{}, vk::SampleCountFlagBits::e1, VK_FALSE, 1.0f, nullptr, VK_FALSE, VK_FALSE};
	_depthStencil = vk::PipelineDepthStencilStateCreateInfo{};
	_colorBlendAttachment = vk::PipelineColorBlendAttachmentState{}; // 默认混合关闭
	_colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
										   vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	_vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{};

	// 动态状态通常是必须的（为了适配不同 Swapchain 尺寸）
	_dynamicStates = {vk ::DynamicState::eViewport, vk::DynamicState::eScissor};
}

PipelineBuilder& PipelineBuilder::setShaders(const vk::raii::ShaderModule& vertShader,
											 const vk::raii::ShaderModule& fragShader) {
	return setShaders(vertShader, fragShader, "vertMain", "fragMain");
}

PipelineBuilder& PipelineBuilder::setShaders(const vk::raii::ShaderModule& vertShader,
											 const vk::raii::ShaderModule& fragShader, const char* vertEntry,
											 const char* fragEntry) {
	_shaderStages.clear();

	vk::PipelineShaderStageCreateInfo vertStageInfo{};
	vertStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	vertStageInfo.module = *vertShader;
	vertStageInfo.pName = vertEntry;
	_shaderStages.push_back(vertStageInfo);

	vk::PipelineShaderStageCreateInfo fragStageInfo{};
	fragStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	fragStageInfo.module = *fragShader;
	fragStageInfo.pName = fragEntry;
	_shaderStages.push_back(fragStageInfo);

	return *this;
}

PipelineBuilder& PipelineBuilder::setInputTopology(vk::PrimitiveTopology topology) {
	_inputAssembly.topology = topology;
	return *this;
}

PipelineBuilder& PipelineBuilder::setPolygonMode(vk::PolygonMode mode) {
	_rasterizer.polygonMode = mode;
	return *this;
}

PipelineBuilder& PipelineBuilder::setCullMode(vk::CullModeFlags cullMode, vk::FrontFace frontFace) {
	_rasterizer.cullMode = cullMode;
	_rasterizer.frontFace = frontFace;
	return *this;
}

// 深度测试配置
PipelineBuilder& PipelineBuilder::setDepthTest(bool depthWriteEnable, bool depthTestEnable, vk::CompareOp compareOp) {
	_depthStencil.depthTestEnable = depthTestEnable ? VK_TRUE : VK_FALSE;
	_depthStencil.depthWriteEnable = depthWriteEnable ? VK_TRUE : VK_FALSE;
	_depthStencil.depthCompareOp = compareOp;
	_depthStencil.minDepthBounds = 0.0f;
	_depthStencil.maxDepthBounds = 1.0f;
	_depthStencil.stencilTestEnable = VK_FALSE;
	return *this;
}

// 简单的透明混合配置
PipelineBuilder& PipelineBuilder::enableAlphaBlending() {
	_colorBlendAttachment.blendEnable = VK_TRUE;
	_colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
	_colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	_colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
	_colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
	_colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
	_colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
	return *this;
}

// 设置 DescriptorSetLayouts 和 PushConstants
PipelineBuilder& PipelineBuilder::setLayout(const std::vector<vk::DescriptorSetLayout>& layouts,
											const std::vector<vk::PushConstantRange>& pushConstants) {
	_descriptorSetLayouts = layouts;
	_pushConstantRanges = pushConstants;
	return *this;
}

// 设置顶点输入格式 (通常来自 Mesh 类的静态方法)
PipelineBuilder& PipelineBuilder::setVertexInput(const vk::PipelineVertexInputStateCreateInfo& info) {
	_vertexInputInfo = info;
	return *this;
}

// 适配 Dynamic Rendering (Vulkan 1.3 或 KHR_dynamic_rendering)
// 如果你不使用 RenderPass 对象，需要这个
PipelineBuilder& PipelineBuilder::setRenderingFormats(const std::vector<vk::Format>& colorFormats,
													  vk::Format depthFormat) {
	_colorAttachmentFormats = colorFormats;
	_depthAttachmentFormat = depthFormat;
	return *this;
}

std::unique_ptr<GraphicsPipeline> PipelineBuilder::build(const vk::raii::Device& device, vk::RenderPass renderPass) {
	// 1. 创建 Pipeline Layout (RAII)
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(_descriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = _descriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(_pushConstantRanges.size());
	pipelineLayoutInfo.pPushConstantRanges = _pushConstantRanges.data();

	// 直接创建 RAII 对象，如果失败会抛出异常 (除非没开启异常)
	vk::raii::PipelineLayout layout(device, pipelineLayoutInfo);

	// 2. 准备各种 State 指针 (与之前相同)
	vk::PipelineViewportStateCreateInfo viewportState{};
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	vk::PipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &_colorBlendAttachment;

	vk::PipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(_dynamicStates.size());
	dynamicStateInfo.pDynamicStates = _dynamicStates.data();

	// 3. 组装 GraphicsPipelineCreateInfo
	vk::GraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.stageCount = static_cast<uint32_t>(_shaderStages.size());
	pipelineInfo.pStages = _shaderStages.data();
	pipelineInfo.pVertexInputState = &_vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &_inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &_rasterizer;
	pipelineInfo.pMultisampleState = &_multisampling;
	pipelineInfo.pDepthStencilState = &_depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.layout = *layout; // 取出刚刚创建的 layout 的句柄
	pipelineInfo.subpass = 0;

	// 处理 Dynamic Rendering vs RenderPass
	vk::PipelineRenderingCreateInfo renderingInfo{};
	if (renderPass) {
		pipelineInfo.renderPass = renderPass;
	} else {
		pipelineInfo.renderPass = nullptr;
		renderingInfo.colorAttachmentCount = static_cast<uint32_t>(_colorAttachmentFormats.size());
		renderingInfo.pColorAttachmentFormats = _colorAttachmentFormats.data();
		renderingInfo.depthAttachmentFormat = _depthAttachmentFormat;
		pipelineInfo.pNext = &renderingInfo;
	}

	// 4. 创建 Pipeline (RAII)
	// device.createGraphicsPipeline 返回的是一个 std::pair<Result, raii::Pipeline> 或者直接是 raii::Pipeline
	// (取决于是否有 pipeline cache) 按照 vulkan_raii 的习惯，这里应该这样写：

	vk::raii::Pipeline pipeline(device,
								nullptr, // pipeline cache, 暂时为空
								pipelineInfo);

	// 5. 返回封装对象
	return std::make_unique<GraphicsPipeline>(std::move(layout), std::move(pipeline));
}

} // namespace Fishy