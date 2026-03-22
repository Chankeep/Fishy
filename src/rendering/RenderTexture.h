#pragma once

#include "core/VulkanDevice.h"
#include "core/VulkanImage.h"

namespace Fishy {

/**
 * @brief A single offscreen render target image.
 *
 * Wraps a VulkanImage with convenience for creating color or depth attachments.
 * Format determines whether this is a color or depth texture.
 */
class RenderTexture {
public:
	RenderTexture(VulkanDevice& device, uint32_t width, uint32_t height, vk::Format format,
				  vk::ImageUsageFlags usage);
	~RenderTexture();

	// Non-copyable, movable
	RenderTexture(const RenderTexture&) = delete;
	RenderTexture& operator=(const RenderTexture&) = delete;
	RenderTexture(RenderTexture&&) noexcept = default;
	RenderTexture& operator=(RenderTexture&&) noexcept = default;

	[[nodiscard]] vk::ImageView getImageView() const;
	[[nodiscard]] vk::Extent2D getExtent() const { return _extent; }
	[[nodiscard]] vk::Format getFormat() const { return _format; }
	[[nodiscard]] vk::Image getImage() const;
	[[nodiscard]] bool isDepthFormat() const;

	/// Destroy and recreate at a new size
	void recreate(uint32_t width, uint32_t height);

private:
	void createResources(uint32_t width, uint32_t height);
	void destroyResources();
	[[nodiscard]] vk::ImageAspectFlags getAspectFlags() const;

	VulkanDevice& _device;
	vk::Format _format;
	vk::ImageUsageFlags _usage;
	std::unique_ptr<VulkanImage> _image;
	vk::Extent2D _extent{0, 0};
};

} // namespace Fishy