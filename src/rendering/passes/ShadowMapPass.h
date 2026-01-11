#pragma once

#include "../IRenderPass.h"
#include "core/VulkanImage.h"
#include "rendering/GraphicsPipeline.h"
#include "rendering/RenderConstants.h"
#include <memory>

namespace Fishy {

class ShadowMapPass : public IRenderPass {
public:
	explicit ShadowMapPass(GraphicsPipeline& pipeline, std::unique_ptr<VulkanImage> shadowMapImage,
						   vk::raii::Sampler sampler)
		: _pipeline(pipeline), _shadowMapImage(std::move(shadowMapImage)), _shadowMapSampler(std::move(sampler)) {}
	~ShadowMapPass() override = default;

	void execute(RenderGraphContext& ctx, entt::registry& registry) override;

	[[nodiscard]] vk::ImageView getShadowMapView() const { return *_shadowMapImage->getView(); }
	[[nodiscard]] vk::Sampler getShadowMapSampler() const { return *_shadowMapSampler; }

	[[nodiscard]] std::string_view getName() const override { return "ShadowMapPass"; }

	// Future DAG support
	[[nodiscard]] std::vector<std::string_view> outputs() const override { return {"ShadowMap"}; }

private:
	GraphicsPipeline& _pipeline;

	std::unique_ptr<VulkanImage> _shadowMapImage;
	vk::raii::Sampler _shadowMapSampler = nullptr;
};
} // namespace Fishy