#pragma once

#include "../IRenderPass.h"

namespace Fishy {

class GraphicsPipeline;

/**
 * @brief Render pass for the main scene using indirect drawing.
 *
 * Renders all visible meshes using the PBR pipeline with bindless textures.
 * Uses indirect draw commands for batched rendering.
 */
class MainScenePass : public IRenderPass {
public:
	explicit MainScenePass(GraphicsPipeline& pipeline) : _pipeline(pipeline) {}
	~MainScenePass() override = default;

	void execute(RenderGraphContext& ctx, entt::registry& registry) override;

	[[nodiscard]] std::string_view getName() const override { return "MainScenePass"; }

	// Future DAG support
	[[nodiscard]] std::vector<std::string_view> outputs() const override {
		return {"ColorAttachment", "DepthAttachment"};
	}

private:
	GraphicsPipeline& _pipeline;
};

} // namespace Fishy
