#pragma once

#include "../core/VulkanBuffer.h"
#include "../core/VulkanDevice.h"

#include <memory>
#include <stb/stb_image.h>
#include <stdexcept>
#include <string>

namespace Fishy {

class Texture {
public:
	Texture(const VulkanDevice& device, const std::string& path);
	~Texture();

	// Prevent copy
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	const vk::raii::Image& getImage() const { return _image; }
	const vk::raii::ImageView& getImageView() const { return _imageView; }
	const vk::raii::Sampler& getSampler() const { return _sampler; }

private:
	void createTextureImage(const std::string& path);
	void createTextureImageView();
	void createTextureSampler();

	const VulkanDevice& _device;

	vk::raii::Image _image = nullptr;
	vk::raii::DeviceMemory _imageMemory = nullptr;
	vk::raii::ImageView _imageView = nullptr;
	vk::raii::Sampler _sampler = nullptr;
};

} // namespace Fishy
