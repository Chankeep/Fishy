#define STB_IMAGE_IMPLEMENTATION
#include "Texture.h"
#include <iostream>

namespace Fishy {

Texture::Texture(const VulkanDevice& device, const std::string& path) : _device(device) {
	createTextureImage(path);
	createTextureImageView();
	createTextureSampler();
}

Texture::Texture(const VulkanDevice& device, const unsigned char* data, size_t size) : _device(device) {
	createTextureImageFromMemory(data, size);
	createTextureImageView();
	createTextureSampler();
}

Texture::Texture(const VulkanDevice& device, const unsigned char* pixels, int width, int height) : _device(device) {
	createTextureFromPixels(pixels, width, height);
	createTextureImageView();
	createTextureSampler();
}

Texture::~Texture() {
	// RAII handles cleanup
}

void Texture::createTextureImage(const std::string& path) {
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		throw std::runtime_error("failed to load texture image: " + path);
	}

	createTextureFromPixels(pixels, texWidth, texHeight);
	stbi_image_free(pixels);
}

void Texture::createTextureImageFromMemory(const unsigned char* data, size_t size) {
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels =
		stbi_load_from_memory(data, static_cast<int>(size), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		throw std::runtime_error("failed to load texture from memory");
	}

	createTextureFromPixels(pixels, texWidth, texHeight);
	stbi_image_free(pixels);
}

void Texture::createTextureFromPixels(const unsigned char* pixels, int texWidth, int texHeight) {
	vk::DeviceSize imageSize = texWidth * texHeight * 4;

	VulkanBuffer stagingBuffer(_device, imageSize, vk::BufferUsageFlagBits::eTransferSrc,
							   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	stagingBuffer.upload((void*)pixels, imageSize);

	vk::ImageCreateInfo imageInfo{
		.imageType = vk::ImageType::e2D,
		.format = vk::Format::eR8G8B8A8Srgb,
		.extent = vk::Extent3D{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		.sharingMode = vk::SharingMode::eExclusive,
		.initialLayout = vk::ImageLayout::eUndefined};

	_image = vk::raii::Image(*_device, imageInfo);

	vk::MemoryRequirements memRequirements = _image.getMemoryRequirements();
	uint32_t memTypeIndex =
		_device.findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::MemoryAllocateInfo allocInfo{
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = memTypeIndex,
	};

	_imageMemory = vk::raii::DeviceMemory(*_device, allocInfo);
	_image.bindMemory(*_imageMemory, 0);

	// Create temporary command pool/buffer for image operations
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

	// Record Layout Transition: Undefined -> TransferDst
	cmd.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

	{
		vk::ImageMemoryBarrier barrier{
			.srcAccessMask = vk::AccessFlagBits::eNone,
			.dstAccessMask = vk::AccessFlagBits::eTransferWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eTransferDstOptimal,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = *_image,
			.subresourceRange =
				{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
		};

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
							vk::DependencyFlags(), nullptr, nullptr, barrier);
	}

	// Copy Buffer to Image
	vk::BufferImageCopy region{
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource =
			{
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.mipLevel = 0,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		.imageOffset = {0, 0, 0},
		.imageExtent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1},
	};

	cmd.copyBufferToImage(*stagingBuffer.getBuffer(), *_image, vk::ImageLayout::eTransferDstOptimal, region);

	// Transition Layout: TransferDst -> ShaderReadOnly
	{
		vk::ImageMemoryBarrier barrier{
			.srcAccessMask = vk::AccessFlagBits::eTransferWrite,
			.dstAccessMask = vk::AccessFlagBits::eShaderRead,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = *_image,
			.subresourceRange =
				{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
		};

		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
							vk::DependencyFlags(), nullptr, nullptr, barrier);
	}

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
		.image = *_image,
		.viewType = vk::ImageViewType::e2D,
		.format = vk::Format::eR8G8B8A8Srgb,
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
