#pragma once

#include "GraphicsPipeline.h"
#include "PipelineManager.h"

namespace Fishy {

class PipelineManager;

class PipelineBuilder {
public:
	explicit PipelineBuilder(vk::Device device);
	PipelineBuilder(const PipelineBuilder&) = delete;
	PipelineBuilder& operator=(const PipelineBuilder&) = delete;

	void clear();

	// Shader configuration
	PipelineBuilder& setShaders(const vk::raii::ShaderModule& vertShader, const vk::raii::ShaderModule& fragShader);
	PipelineBuilder& setShaders(const vk::raii::ShaderModule& vertShader, const vk::raii::ShaderModule& fragShader,
								const char* vertEntry, const char* fragEntry);

	/**
	 * @brief Set shader identifier for pipeline caching.
	 * @param shaderId Pre-hashed shader ID (e.g., entt::hashed_string{"shaders/PBR.slang"}.value())
	 * @param variantId Optional variant ID for shader permutations (default: 0)
	 */
	PipelineBuilder& setShaderId(entt::id_type shaderId, entt::id_type variantId = 0);

	// Pipeline state configuration
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

	/**
	 * @brief Generate a PipelineKey for caching based on current configuration.
	 */
	[[nodiscard]] PipelineKey generateKey() const;

	/**
	 * @brief Build pipeline with caching through PipelineManager.
	 *
	 * This is the preferred way to create pipelines. The manager will:
	 * 1. Check if a pipeline with the same key already exists
	 * 2. Return cached pipeline if found
	 * 3. Create new pipeline using this builder if not found
	 *
	 * @param manager The pipeline manager for caching.
	 * @return Pointer to the (cached or new) pipeline.
	 * @throws std::runtime_error if creation fails.
	 */
	[[nodiscard]] GraphicsPipeline* build(PipelineManager& manager);

	/**
	 * @brief Build pipeline directly without caching (legacy API).
	 */
	[[nodiscard]] std::unique_ptr<GraphicsPipeline> build(const vk::raii::Device& device, vk::RenderPass renderPass,
														  const vk::raii::PipelineCache& pipelineCache);

private:
	std::vector<vk::PipelineShaderStageCreateInfo> _shaderStages;
	std::vector<vk::DescriptorSetLayout> _descriptorSetLayouts;
	std::vector<vk::PushConstantRange> _pushConstantRanges;
	std::vector<vk::DynamicState> _dynamicStates;
	std::vector<vk::Format> _colorAttachmentFormats;
	vk::Format _depthAttachmentFormat = vk::Format::eUndefined;

	vk::PipelineInputAssemblyStateCreateInfo _inputAssembly;
	vk::PipelineRasterizationStateCreateInfo _rasterizer;
	vk::PipelineMultisampleStateCreateInfo _multisampling;
	vk::PipelineDepthStencilStateCreateInfo _depthStencil;
	vk::PipelineColorBlendAttachmentState _colorBlendAttachment;
	vk::PipelineVertexInputStateCreateInfo _vertexInputInfo;

	// For generateKey()
	entt::id_type _shaderId = 0;
	entt::id_type _variantId = 0;
};
} // namespace Fishy
