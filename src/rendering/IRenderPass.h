#pragma once

#include "DrawTypes.h"
#include "core/VulkanBuffer.h"
#include "ecs/systems/RenderSystem.h"
#include "resources/IBLEnvironment.h"

#include <entt/entt.hpp>
#include <span>
#include <string_view>
#include <vector>

namespace Fishy {

class GraphicsPipeline;

/**
 * @brief Context passed to each RenderPass during execution.
 *
 * Contains borrowed views of all data needed for rendering. All pointers and spans
 * are non-owning - the Renderer maintains ownership of the underlying data.
 */
struct RenderGraphContext {
	// Command buffer for recording draw commands
	vk::raii::CommandBuffer& cmd;

	// Viewport dimensions
	vk::Extent2D viewportExtent;

	// Camera and light parameters (from ECS)
	const RenderParams& params;

	// Descriptor Sets
	vk::DescriptorSet globalDescriptorSet; // Set 0: IBL textures
	vk::DescriptorSet bindlessTextureSet;  // Set 1: Bindless textures

	// Buffer Device Addresses for push constants
	uint64_t instanceDataAddress = 0;
	uint64_t globalDataAddress = 0;
	uint64_t lightDataAddress = 0;
	uint64_t shadowDataAddress = 0;
	uint32_t shadowCasterCount = 0;

	// === Scene Geometry (borrowed, non-owning, per-frame) ===
	std::span<const DrawBatch> drawBatches;
	VulkanBuffer* indirectBuffer = nullptr; // Current frame's indirect buffer
	VulkanBuffer* vertexBuffer = nullptr;	// Unified vertex buffer
	VulkanBuffer* indexBuffer = nullptr;	// Unified index buffer

	// IBL environment for skybox rendering
	IBLEnvironment* iblEnvironment = nullptr;

	// Render attachments for passes that manage their own beginRendering/endRendering
	vk::ImageView colorAttachmentView = nullptr;
	vk::ImageView depthAttachmentView = nullptr;
};

/**
 * @brief Abstract base class for render passes.
 *
 * Each pass encapsulates a specific rendering operation (e.g., main scene, skybox).
 * Passes are stateless and receive all required data via RenderGraphContext.
 */
class IRenderPass {
public:
	virtual ~IRenderPass() = default;

	/**
	 * @brief Execute this render pass.
	 * @param ctx Context containing all borrowed data for rendering.
	 * @param registry ECS registry for entity queries (if needed).
	 */
	virtual void execute(RenderGraphContext& ctx, entt::registry& registry) = 0;

	/**
	 * @brief Get the name of this pass for debugging.
	 */
	[[nodiscard]] virtual std::string_view getName() const = 0;

	// === Future DAG support (placeholders) ===
	// Returns resource names this pass reads from
	[[nodiscard]] virtual std::vector<std::string_view> inputs() const { return {}; }
	// Returns resource names this pass writes to
	[[nodiscard]] virtual std::vector<std::string_view> outputs() const { return {}; }
};

} // namespace Fishy
