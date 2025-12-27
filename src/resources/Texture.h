#pragma once

#include "../core/VulkanBuffer.h"
#include "../core/VulkanDevice.h"

#include <memory>

#if (__linux__)
#include <stb/stb_image.h>
#else
#include <stb_image.h>
#endif

#include <stdexcept>
#include <string>

namespace Fishy {

class Texture {
public:
	// Load from file path
	// format: eR8G8B8A8Srgb for color textures, eR8G8B8A8Unorm for data textures
	Texture(const VulkanDevice& device, const std::string& path, vk::Format format = vk::Format::eR8G8B8A8Srgb);

	// Load from encoded image data in memory (PNG, JPG, etc. for embedded glTF textures)
	Texture(const VulkanDevice& device, const unsigned char* data, size_t size,
			vk::Format format = vk::Format::eR8G8B8A8Srgb);

	// Create from raw RGBA pixel data (for programmatic textures like default white/normal)
	Texture(const VulkanDevice& device, const unsigned char* pixels, int width, int height,
			vk::Format format = vk::Format::eR8G8B8A8Srgb);

	~Texture();

	// Prevent copy
	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	VkImage getImage() const { return _image; }
	const vk::raii::ImageView& getImageView() const { return _imageView; }
	const vk::raii::Sampler& getSampler() const { return _sampler; }

private:
	void transitionImageLayout(vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
	void copyBufferToImage(const VulkanBuffer& buffer, uint32_t width, uint32_t height);

private:
	void createTextureImage(const std::string& path);
	void createTextureImageFromMemory(const unsigned char* data, size_t size);
	void createTextureFromPixels(const unsigned char* pixels, int texWidth, int texHeight);
	void createTextureImageView();
	void createTextureSampler();

	const VulkanDevice& _device;
	vk::Format _format;

	VkImage _image = VK_NULL_HANDLE;
	VmaAllocation _vmaAllocation = nullptr;
	VmaAllocator _vmaAllocator = nullptr;
	vk::raii::ImageView _imageView = nullptr;
	vk::raii::Sampler _sampler = nullptr;
};

} // namespace Fishy
