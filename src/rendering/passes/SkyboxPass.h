#pragma once

#include "../IRenderPass.h"
#include "core/VulkanBuffer.h"

#include <memory>

namespace Fishy {

class GraphicsPipeline;
class VulkanDevice;

/**
 * @brief Render pass for the skybox.
 *
 * Renders the environment skybox using the IBL cubemap.
 * Owns skybox geometry buffers (static, created once).
 */
class SkyboxPass : public IRenderPass {
public:
	/**
	 * @brief Construct SkyboxPass with pipeline and geometry.
	 * @param pipeline Reference to the skybox pipeline.
	 * @param vertexBuffer Skybox vertex buffer (ownership transferred).
	 * @param indexBuffer Skybox index buffer (ownership transferred).
	 * @param indexCount Number of indices in the skybox mesh.
	 */
	SkyboxPass(GraphicsPipeline& pipeline, std::unique_ptr<VulkanBuffer> vertexBuffer,
			   std::unique_ptr<VulkanBuffer> indexBuffer, uint32_t indexCount)
		: _pipeline(pipeline), _vertexBuffer(std::move(vertexBuffer)), _indexBuffer(std::move(indexBuffer)),
		  _indexCount(indexCount) {}

	~SkyboxPass() override = default;

	void execute(RenderGraphContext& ctx, entt::registry& registry) override;

	[[nodiscard]] std::string_view getName() const override { return "SkyboxPass"; }

	// Future DAG support
	[[nodiscard]] std::vector<std::string_view> inputs() const override { return {"DepthAttachment"}; }
	[[nodiscard]] std::vector<std::string_view> outputs() const override { return {"ColorAttachment"}; }

private:
	GraphicsPipeline& _pipeline;
	std::unique_ptr<VulkanBuffer> _vertexBuffer;
	std::unique_ptr<VulkanBuffer> _indexBuffer;
	uint32_t _indexCount;
};

} // namespace Fishy
