#pragma once

#include "../IRenderPass.h"
#include "rendering/RenderTexture.h"
#include "rendering/GraphicsPipeline.h"
#include "rendering/RenderConstants.h"
#include <memory>

namespace Fishy {

class ShadowMapPass : public IRenderPass {
public:
	explicit ShadowMapPass(GraphicsPipeline& pipeline, std::unique_ptr<RenderTexture> shadowMap,
						   vk::raii::Sampler sampler)
		: _pipeline(pipeline), _shadowMap(std::move(shadowMap)), _shadowMapSampler(std::move(sampler)) {}
	~ShadowMapPass() override = default;

	void execute(RenderGraphContext& ctx, entt::registry& registry) override;

	[[nodiscard]] vk::ImageView getShadowMapView() const { return _shadowMap->getImageView(); }
	[[nodiscard]] vk::Sampler getShadowMapSampler() const { return *_shadowMapSampler; }

	[[nodiscard]] std::string_view getName() const override { return "ShadowMapPass"; }

	// Future DAG support
	[[nodiscard]] std::vector<std::string_view> outputs() const override { return {"ShadowMap"}; }

private:
	GraphicsPipeline& _pipeline;

	std::unique_ptr<RenderTexture> _shadowMap;
	vk::raii::Sampler _shadowMapSampler = nullptr;
};
} // namespace Fishy