#define STB_IMAGE_IMPLEMENTATION
#include "Texture.h"
#include <iostream>

namespace Fishy {

Texture::Texture(const VulkanDevice& device, const std::string& path, vk::Format format)
	: _device(device), _format(format), _vmaAllocator(device.getVmaAllocator()) {
	createTextureImage(path);
	createTextureImageView();
	createTextureSampler();
}

Texture::Texture(const VulkanDevice& device, const unsigned char* data, size_t size, vk::Format format)
	: _device(device), _format(format), _vmaAllocator(device.getVmaAllocator()) {
	createTextureImageFromMemory(data, size);
	createTextureImageView();
	createTextureSampler();
}

Texture::Texture(const VulkanDevice& device, const unsigned char* pixels, int width, int height, vk::Format format)
	: _device(device), _format(format), _vmaAllocator(device.getVmaAllocator()) {
	createTextureFromPixels(pixels, width, height);
	createTextureImageView();
	createTextureSampler();
}

Texture::~Texture() {
	if (_vmaAllocation && _vmaAllocator) {
		vmaDestroyImage(_vmaAllocator, _image, _vmaAllocation);
	}
	// RAII handles imageView and sampler cleanup
}

void Texture::createTextureImage(const std::string& path) {
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		LogSystem::get().error("Failed to load texture image: {}", path);
		throw std::runtime_error("failed to load texture image: " + path);
	}

	LogSystem::get().trace("Loaded texture from file: {} ({}x{} {} channels)",
		path, texWidth, texHeight, texChannels);
	createTextureFromPixels(pixels, texWidth, texHeight);
	stbi_image_free(pixels);
}

void Texture::createTextureImageFromMemory(const unsigned char* data, size_t size) {
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels =
		stbi_load_from_memory(data, static_cast<int>(size), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		LogSystem::get().error("Failed to load texture from memory ({} bytes)", size);
		throw std::runtime_error("failed to load texture from memory");
	}

	LogSystem::get().trace("Loaded texture from memory: {}x{} {} channels ({} bytes)",
		texWidth, texHeight, texChannels, size);
	createTextureFromPixels(pixels, texWidth, texHeight);
	stbi_image_free(pixels);
}

void Texture::createTextureFromPixels(const unsigned char* pixels, int texWidth, int texHeight) {
	vk::DeviceSize imageSize = texWidth * texHeight * 4;

	VulkanBuffer stagingBuffer(_device, imageSize, vk::BufferUsageFlagBits::eTransferSrc,
							   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	stagingBuffer.upload((void*)pixels, imageSize);

	// Create Image using VMA (keep vk:: style, convert to Vk for VMA)
	vk::ImageCreateInfo imageInfo{
		.imageType = vk::ImageType::e2D,
		.format = _format,
		.extent = vk::Extent3D{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		.sharingMode = vk::SharingMode::eExclusive,
		.initialLayout = vk::ImageLayout::eUndefined};

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkResult result = vmaCreateImage(_vmaAllocator, reinterpret_cast<const VkImageCreateInfo*>(&imageInfo), &allocInfo,
									 &_image, &_vmaAllocation, nullptr);

	if (result != VK_SUCCESS) {
		LogSystem::get().error("Failed to create VMA image: {}x{}", texWidth, texHeight);
		throw std::runtime_error("Failed to create VMA image!");
	}

	LogSystem::get().trace("VMA created texture image handle: {}", reinterpret_cast<uintptr_t>(_image));

	// Transition layout and copy buffer to image
	transitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
	copyBufferToImage(stagingBuffer, texWidth, texHeight);
	transitionImageLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
}

void Texture::transitionImageLayout(vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
	vk::CommandPoolCreateInfo poolInfo{
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = _device.getGraphicsQueueFamilyIndex(),
	};
	vk::raii::CommandPool commandPool(*_device, poolInfo);

	vk::CommandBufferAllocateInfo cmdAllocInfo{
		.commandPool = *commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	};

	vk::raii::CommandBuffers cmdbuffers(*_device, cmdAllocInfo);
	vk::raii::CommandBuffer& cmd = cmdbuffers[0];

	cmd.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

	vk::PipelineStageFlags srcStage;
	vk::PipelineStageFlags dstStage;

	vk::AccessFlags srcAccess;
	vk::AccessFlags dstAccess;

	// Determine stage and access masks based on layouts
	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
		srcAccess = vk::AccessFlagBits::eNone;
		dstAccess = vk::AccessFlagBits::eTransferWrite;
		srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits::eTransfer;
	} else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
		srcAccess = vk::AccessFlagBits::eTransferWrite;
		dstAccess = vk::AccessFlagBits::eShaderRead;
		srcStage = vk::PipelineStageFlagBits::eTransfer;
		dstStage = vk::PipelineStageFlagBits::eFragmentShader;
	} else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	vk::ImageMemoryBarrier barrier{
		.srcAccessMask = srcAccess,
		.dstAccessMask = dstAccess,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = _image,
		.subresourceRange = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	cmd.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags(), nullptr, nullptr, barrier);

	cmd.end();

	vk::SubmitInfo submitInfo{
		.commandBufferCount = 1,
		.pCommandBuffers = &(*cmd),
	};

	_device.getGraphicsQueue().submit(submitInfo, nullptr);
	_device.getGraphicsQueue().waitIdle();
}

void Texture::copyBufferToImage(const VulkanBuffer& buffer, uint32_t width, uint32_t height) {
	vk::CommandPoolCreateInfo poolInfo{
		.flags = vk::CommandPoolCreateFlagBits::eTransient,
		.queueFamilyIndex = _device.getGraphicsQueueFamilyIndex(),
	};
	vk::raii::CommandPool commandPool(*_device, poolInfo);

	vk::CommandBufferAllocateInfo cmdAllocInfo{
		.commandPool = *commandPool,
		.level = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1,
	};

	vk::raii::CommandBuffers cmdbuffers(*_device, cmdAllocInfo);
	vk::raii::CommandBuffer& cmd = cmdbuffers[0];

	cmd.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

	vk::BufferImageCopy region{
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.mipLevel = 0,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
		.imageOffset = {0, 0, 0},
		.imageExtent = {width, height, 1},
	};

	cmd.copyBufferToImage(buffer.getBuffer(), _image, vk::ImageLayout::eTransferDstOptimal, region);

	cmd.end();

	vk::SubmitInfo submitInfo{
		.commandBufferCount = 1,
		.pCommandBuffers = &(*cmd),
	};

	_device.getGraphicsQueue().submit(submitInfo, nullptr);
	_device.getGraphicsQueue().waitIdle();
}

void Texture::createTextureImageView() {
	vk::ImageViewCreateInfo viewInfo{
		.image = _image,
		.viewType = vk::ImageViewType::e2D,
		.format = _format,
		.subresourceRange =
			{
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
	};

	_imageView = vk::raii::ImageView(*_device, viewInfo);
}

void Texture::createTextureSampler() {
	auto properties = _device.getPhysicalDevice().getProperties();
	vk::SamplerCreateInfo samplerInfo{
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = vk::SamplerAddressMode::eRepeat,
		.addressModeV = vk::SamplerAddressMode::eRepeat,
		.addressModeW = vk::SamplerAddressMode::eRepeat,
		.mipLodBias = 0.0f,
		.anisotropyEnable = vk::True,
		.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
		.compareEnable = vk::False,
		.compareOp = vk::CompareOp::eAlways,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = vk::BorderColor::eIntOpaqueBlack,
		.unnormalizedCoordinates = vk::False,
	};

	_sampler = vk::raii::Sampler(*_device, samplerInfo);
}

} // namespace Fishy
