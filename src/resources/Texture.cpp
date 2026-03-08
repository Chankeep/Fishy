#define STB_IMAGE_IMPLEMENTATION
#include "Texture.h"
#include "../core/VulkanUtils.h"

namespace Fishy {

Texture::Texture(const VulkanDevice& device, const std::string& path, vk::Format format)
	: _device(device), _format(format) {
#ifndef NDEBUG
	_debugName = path;
#endif
	createTextureImage(path);
	createTextureImageView();
	createTextureSampler();
}

Texture::Texture(const VulkanDevice& device, const unsigned char* data, size_t size, vk::Format format)
	: _device(device), _format(format) {
#ifndef NDEBUG
	static int embeddedId = 0;
	_debugName = "Texture_Embedded_" + std::to_string(embeddedId++);
#endif
	createTextureImageFromMemory(data, size);
	createTextureImageView();
	createTextureSampler();
}

Texture::Texture(const VulkanDevice& device, const unsigned char* pixels, int width, int height, vk::Format format)
	: _device(device), _format(format) {
#ifndef NDEBUG
	_debugName = "Texture_Generated_" + std::to_string(width) + "x" + std::to_string(height);
#endif
	createTextureFromPixels(pixels, width, height);
	createTextureImageView();
	createTextureSampler();
}

Texture::~Texture() {
	// RAII handles all cleanup via VulkanImage and Sampler destructors
}

void Texture::createTextureImage(const std::string& path) {
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		FISHY_LOG_ERROR("Failed to load texture image: {}", path);
		throw std::runtime_error("failed to load texture image: " + path);
	}

	FISHY_LOG_TRACE("Loaded texture from file: {} ({}x{} {} channels)", path, texWidth, texHeight, texChannels);
	createTextureFromPixels(pixels, texWidth, texHeight);
	stbi_image_free(pixels);
}

void Texture::createTextureImageFromMemory(const unsigned char* data, size_t size) {
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels =
		stbi_load_from_memory(data, static_cast<int>(size), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		FISHY_LOG_ERROR("Failed to load texture from memory ({} bytes)", size);
		throw std::runtime_error("failed to load texture from memory");
	}

	FISHY_LOG_TRACE("Loaded texture from memory: {}x{} {} channels ({} bytes)", texWidth, texHeight, texChannels,
						   size);
	createTextureFromPixels(pixels, texWidth, texHeight);
	stbi_image_free(pixels);
}

// Helper to get bytes per pixel for a format
static size_t getBytesPerPixel(vk::Format format) {
	switch (format) {
	case vk::Format::eR8G8B8A8Srgb:
	case vk::Format::eR8G8B8A8Unorm:
		return 4;
	case vk::Format::eR16G16Sfloat:
		return 4; // 2 bytes × 2 channels
	case vk::Format::eR16G16B16A16Sfloat:
		return 8; // 2 bytes × 4 channels
	default:
		FISHY_LOG_WARN("Unknown format in getBytesPerPixel, assuming 4 bytes per pixel");
		return 4;
	}
}

void Texture::createTextureFromPixels(const unsigned char* pixels, int texWidth, int texHeight) {
	vk::DeviceSize imageSize = texWidth * texHeight * getBytesPerPixel(_format);

	VulkanBuffer stagingBuffer(_device, imageSize, vk::BufferUsageFlagBits::eTransferSrc,
							   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	stagingBuffer.upload(pixels, imageSize);

	// Create Image using VulkanImage wrapper
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

	_image = std::make_unique<VulkanImage>(_device.getVmaAllocator(), imageInfo, allocInfo);

	FISHY_LOG_TRACE("VMA created texture image handle: {}",
						   reinterpret_cast<uintptr_t>(static_cast<VkImage>(_image->getImage())));

#ifndef NDEBUG
	VulkanUtils::setDebugName(_device, _image->getImage(), _debugName.c_str());
#endif

	// Transition layout and copy buffer to image
	transitionImageLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
	copyBufferToImage(stagingBuffer, texWidth, texHeight);
	transitionImageLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
}

void Texture::transitionImageLayout(vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
	vk::PipelineStageFlags2 srcStage;
	vk::PipelineStageFlags2 dstStage;
	vk::AccessFlags2 srcAccess;
	vk::AccessFlags2 dstAccess;

	// Determine stage and access masks based on layouts
	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
		srcAccess = vk::AccessFlagBits2::eNone;
		dstAccess = vk::AccessFlagBits2::eTransferWrite;
		srcStage = vk::PipelineStageFlagBits2::eTopOfPipe;
		dstStage = vk::PipelineStageFlagBits2::eTransfer;
	} else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
			   newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
		srcAccess = vk::AccessFlagBits2::eTransferWrite;
		dstAccess = vk::AccessFlagBits2::eShaderRead;
		srcStage = vk::PipelineStageFlagBits2::eTransfer;
		dstStage = vk::PipelineStageFlagBits2::eFragmentShader;
	} else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	VulkanUtils::executeImmediate(_device, [&](auto& cmd) {
		VulkanUtils::transitionImage(*cmd, _image->getImage(), oldLayout, newLayout, srcAccess, dstAccess, srcStage,
									 dstStage, vk::ImageAspectFlagBits::eColor);
	});
}

void Texture::copyBufferToImage(const VulkanBuffer& buffer, uint32_t width, uint32_t height) {
	VulkanUtils::executeImmediate(_device, [&](auto& cmd) {
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
			.imageExtent = {width, height, 1},
		};

		cmd.copyBufferToImage(buffer.getBuffer(), _image->getImage(), vk::ImageLayout::eTransferDstOptimal, region);
	});
}

void Texture::createTextureImageView() {
	vk::ImageViewCreateInfo viewInfo{
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

	_image->createView(*_device, viewInfo);

#ifndef NDEBUG
	VulkanUtils::setDebugName(_device, *_image->getView(), vk::ObjectType::eImageView, (_debugName + "_View").c_str());
#endif
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

#ifndef NDEBUG
	VulkanUtils::setDebugName(_device, *_sampler, vk::ObjectType::eSampler, (_debugName + "_Sampler").c_str());
#endif
}

} // namespace Fishy
