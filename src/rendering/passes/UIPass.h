#pragma once

#include "../IRenderPass.h"

#include <functional>
#include <vulkan/vulkan.hpp>

namespace Fishy {

/**
 * @brief Render pass for UI overlay (ImGui).
 *
 * Handles UI rendering with its own render pass (load existing color, no depth).
 * This pass manages its own beginRendering/endRendering because UI rendering
 * uses different attachment settings (load instead of clear, no depth).
 */
class UIPass : public IRenderPass {
public:
	using UICallback = std::function<void(VkCommandBuffer)>;

	/**
	 * @brief Construct UIPass with optional callback.
	 * @param callback UI rendering callback (can be nullptr, set per-frame via setCallback).
	 */
	explicit UIPass(UICallback callback = nullptr) : _callback(std::move(callback)) {}

	~UIPass() override = default;

	/**
	 * @brief Set or update the UI callback for this frame.
	 * @param callback New callback function.
	 */
	void setCallback(UICallback callback) { _callback = std::move(callback); }

	/**
	 * @brief Set the current frame's image view for UI rendering.
	 * Must be called each frame before execute().
	 */
	void setImageView(vk::ImageView imageView) { _currentImageView = imageView; }

	/**
	 * @brief Check if this pass has a valid callback.
	 */
	[[nodiscard]] bool hasCallback() const { return static_cast<bool>(_callback); }

	void execute(RenderGraphContext& ctx, entt::registry& registry) override;

	[[nodiscard]] std::string_view getName() const override { return "UIPass"; }

	// Future DAG support
	[[nodiscard]] std::vector<std::string_view> inputs() const override { return {"ColorAttachment"}; }
	[[nodiscard]] std::vector<std::string_view> outputs() const override { return {"ColorAttachment"}; }

private:
	UICallback _callback;
	vk::ImageView _currentImageView;
};

} // namespace Fishy
