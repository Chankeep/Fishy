#pragma once

#include "../core/VulkanDevice.h"
#include "../core/VulkanImage.h"

#include <memory>
#include <string>

namespace Fishy {

/**
 * @brief Cubemap texture for IBL (Image-Based Lighting).
 *
 * Loads KTX2 cubemap textures with support for mipmaps.
 * Used for irradiance and pre-filtered environment maps.
 */
class CubemapTexture {
public:
	/**
	 * @brief Load a cubemap from a KTX2 file.
	 * @param device Vulkan device reference.
	 * @param path Path to the KTX2 file.
	 */
	CubemapTexture(const VulkanDevice& device, const std::string& path);
	~CubemapTexture();

	// Prevent copy
	CubemapTexture(const CubemapTexture&) = delete;
	CubemapTexture& operator=(const CubemapTexture&) = delete;

	/**
	 * @brief Create a minimal 1x1 black cubemap for placeholder bindings.
	 */
	[[nodiscard]] static std::shared_ptr<CubemapTexture> createDefault(const VulkanDevice& device);

	[[nodiscard]] vk::Image getImage() const { return _image->getImage(); }
	[[nodiscard]] const vk::raii::ImageView& getImageView() const { return _image->getView(); }
	[[nodiscard]] const vk::raii::Sampler& getSampler() const { return _sampler; }
	[[nodiscard]] uint32_t getMipLevels() const { return _mipLevels; }

private:
	// Private constructor for createDefault factory
	CubemapTexture(const VulkanDevice& device);

	void loadFromKTX2(const std::string& path);
	void createFromData(const uint8_t* data, uint32_t width, vk::Format format);
	void createImageView();
	void createSampler();

	const VulkanDevice& _device;
	vk::Format _format = vk::Format::eUndefined;
	uint32_t _width = 0;
	uint32_t _height = 0;
	uint32_t _mipLevels = 1;

	std::unique_ptr<VulkanImage> _image;
	vk::raii::Sampler _sampler = nullptr;
#ifndef NDEBUG
	std::string _debugName;
#endif
};

} // namespace Fishy
