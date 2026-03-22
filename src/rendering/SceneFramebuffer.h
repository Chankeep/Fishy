#pragma once

#include "RenderTexture.h"

#include <volk.h>

namespace Fishy {

/**
 * @brief Offscreen framebuffer for the editor Viewport.
 *
 * Composes a color RenderTexture + depth RenderTexture, and registers
 * the color attachment with ImGui for display via ImGui::Image().
 */
class SceneFramebuffer {
public:
	SceneFramebuffer(VulkanDevice& device, vk::Format colorFormat,
					 uint32_t width = 1280, uint32_t height = 720);
	~SceneFramebuffer();

	// Non-copyable
	SceneFramebuffer(const SceneFramebuffer&) = delete;
	SceneFramebuffer& operator=(const SceneFramebuffer&) = delete;

	void resize(uint32_t width, uint32_t height);
	[[nodiscard]] bool needsResize(uint32_t w, uint32_t h) const;

	[[nodiscard]] VkDescriptorSet getImGuiTextureID() const { return _imguiDescriptor; }
	[[nodiscard]] RenderTexture& getColorTexture() { return _colorTexture; }
	[[nodiscard]] RenderTexture& getDepthTexture() { return _depthTexture; }
	[[nodiscard]] const RenderTexture& getColorTexture() const { return _colorTexture; }
	[[nodiscard]] const RenderTexture& getDepthTexture() const { return _depthTexture; }
	[[nodiscard]] vk::Extent2D getExtent() const { return _colorTexture.getExtent(); }

private:
	void createSampler();
	void registerImGuiTexture();
	void unregisterImGuiTexture();

	VulkanDevice& _device;
	RenderTexture _colorTexture;
	RenderTexture _depthTexture;

	VkDescriptorSet _imguiDescriptor = VK_NULL_HANDLE;
	vk::raii::Sampler _sampler = nullptr;
};

} // namespace Fishy