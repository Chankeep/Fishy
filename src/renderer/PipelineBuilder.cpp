#include "PipelineBuilder.h"

namespace Fishy {

PipelineBuilder::PipelineBuilder(vk::Device /*device*/) { clear(); }

void PipelineBuilder::clear() {
	_shaderStages.clear();
	_inputAssembly = {.topology = vk::PrimitiveTopology::eTriangleList, .primitiveRestartEnable = VK_FALSE};

	_rasterizer = {.depthClampEnable = VK_FALSE,
				   .rasterizerDiscardEnable = VK_FALSE,
				   .polygonMode = vk::PolygonMode::eFill,
				   .cullMode = vk::CullModeFlagBits::eBack,
				   .frontFace = vk::FrontFace::eCounterClockwise,
				   .depthBiasEnable = VK_FALSE,
				   .depthBiasConstantFactor = 0.0f,
				   .depthBiasClamp = 0.0f,
				   .depthBiasSlopeFactor = 0.0f,
				   .lineWidth = 1.0f};

	_multisampling = {.rasterizationSamples = vk::SampleCountFlagBits::e1,
					  .sampleShadingEnable = VK_FALSE,
					  .minSampleShading = 1.0f,
					  .pSampleMask = nullptr,
					  .alphaToCoverageEnable = VK_FALSE,
					  .alphaToOneEnable = VK_FALSE};

	_depthStencil = {};

	_colorBlendAttachment = vk::PipelineColorBlendAttachmentState{}; // Not a CreateInfo, no sType
	// Default blending disabled
	_colorBlendAttachment.blendEnable = VK_FALSE;
	_colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
										   vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

	_vertexInputInfo = {};

	// Dynamic states are usually required (to adapt to different Swapchain sizes)
	_dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
}

PipelineBuilder& PipelineBuilder::setShaders(const vk::raii::ShaderModule& vertShader,
											 const vk::raii::ShaderModule& fragShader) {
	return setShaders(vertShader, fragShader, "vertMain", "fragMain");
}

PipelineBuilder& PipelineBuilder::setShaders(const vk::raii::ShaderModule& vertShader,
											 const vk::raii::ShaderModule& fragShader, const char* vertEntry,
											 const char* fragEntry) {
	_shaderStages.clear();

	vk::PipelineShaderStageCreateInfo vertStageInfo{
		.stage = vk::ShaderStageFlagBits::eVertex, .module = *vertShader, .pName = vertEntry};
	_shaderStages.push_back(vertStageInfo);

	vk::PipelineShaderStageCreateInfo fragStageInfo{
		.stage = vk::ShaderStageFlagBits::eFragment, .module = *fragShader, .pName = fragEntry};
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

// Depth Test Configuration
PipelineBuilder& PipelineBuilder::setDepthTest(bool depthWriteEnable, bool depthTestEnable, vk::CompareOp compareOp) {
	_depthStencil.depthTestEnable = depthTestEnable ? VK_TRUE : VK_FALSE;
	_depthStencil.depthWriteEnable = depthWriteEnable ? VK_TRUE : VK_FALSE;
	_depthStencil.depthCompareOp = compareOp;
	_depthStencil.minDepthBounds = 0.0f;
	_depthStencil.maxDepthBounds = 1.0f;
	_depthStencil.stencilTestEnable = VK_FALSE;
	return *this;
}

// Simple Alpha Blending Configuration
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

// Set DescriptorSetLayouts and PushConstants
PipelineBuilder& PipelineBuilder::setLayout(const std::vector<vk::DescriptorSetLayout>& layouts,
											const std::vector<vk::PushConstantRange>& pushConstants) {
	_descriptorSetLayouts = layouts;
	_pushConstantRanges = pushConstants;
	return *this;
}

// Set Vertex Input Format (usually from Mesh class static methods)
PipelineBuilder& PipelineBuilder::setVertexInput(const vk::PipelineVertexInputStateCreateInfo& info) {
	_vertexInputInfo = info;
	return *this;
}

// Adapt for Dynamic Rendering (Vulkan 1.3 or KHR_dynamic_rendering)
// If you don't use RenderPass object, you need this
PipelineBuilder& PipelineBuilder::setRenderingFormats(const std::vector<vk::Format>& colorFormats,
													  vk::Format depthFormat) {
	_colorAttachmentFormats = colorFormats;
	_depthAttachmentFormat = depthFormat;
	return *this;
}

std::unique_ptr<GraphicsPipeline> PipelineBuilder::build(const vk::raii::Device& device, vk::RenderPass renderPass) {
	// 1. Create Pipeline Layout (RAII)
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
		.setLayoutCount = static_cast<uint32_t>(_descriptorSetLayouts.size()),
		.pSetLayouts = _descriptorSetLayouts.data(),
		.pushConstantRangeCount = static_cast<uint32_t>(_pushConstantRanges.size()),
		.pPushConstantRanges = _pushConstantRanges.data()};

	// Create RAII object directly, will throw exception if failed (unless exceptions disabled)
	vk::raii::PipelineLayout layout(device, pipelineLayoutInfo);

	// 2. Prepare state pointers (Same as before)
	vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

	vk::PipelineColorBlendStateCreateInfo colorBlending{
		.logicOpEnable = VK_FALSE, .attachmentCount = 1, .pAttachments = &_colorBlendAttachment};

	vk::PipelineDynamicStateCreateInfo dynamicStateInfo{
		.dynamicStateCount = static_cast<uint32_t>(_dynamicStates.size()), .pDynamicStates = _dynamicStates.data()};

	// 3. Assemble GraphicsPipelineCreateInfo
	vk::GraphicsPipelineCreateInfo pipelineInfo{.stageCount = static_cast<uint32_t>(_shaderStages.size()),
												.pStages = _shaderStages.data(),
												.pVertexInputState = &_vertexInputInfo,
												.pInputAssemblyState = &_inputAssembly,
												.pViewportState = &viewportState,
												.pRasterizationState = &_rasterizer,
												.pMultisampleState = &_multisampling,
												.pDepthStencilState = &_depthStencil,
												.pColorBlendState =
													&colorBlending, // Pointer to struct, not struct itself
												.pDynamicState = &dynamicStateInfo,
												.layout = *layout, // Get handle from just created layout
												.subpass = 0};

	// Handle Dynamic Rendering vs RenderPass
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

	// 4. Create Pipeline (RAII)
	// device.createGraphicsPipeline returns a std::pair<Result, raii::Pipeline> or directly raii::Pipeline
	// (depending on if pipeline cache is used) As per vulkan_raii convention, it should be like this:

	vk::raii::Pipeline pipeline(device,
								nullptr, // pipeline cache, empty for now
								pipelineInfo);

	// 5. Return wrapper object
	return std::make_unique<GraphicsPipeline>(std::move(layout), std::move(pipeline));
}

} // namespace Fishy