#include "CubemapTexture.h"
#include "../core/CommandPool.h"
#include "../core/VulkanBuffer.h"

#include <cstring>
#include <ktx.h>
#include <ktxvulkan.h>
#include <stdexcept>

namespace Fishy {

CubemapTexture::CubemapTexture(const VulkanDevice& device, const std::string& path) : _device(device) {
	loadFromKTX2(path);
	createImageView();
	createSampler();
}

// Private constructor for createDefault factory
CubemapTexture::CubemapTexture(const VulkanDevice& device) : _device(device) {}

CubemapTexture::~CubemapTexture() {
	if (_vmaAllocation && _device.getVmaAllocator()) {
		vmaDestroyImage(_device.getVmaAllocator(), _image, _vmaAllocation);
	}
}

void CubemapTexture::loadFromKTX2(const std::string& path) {
	ktxTexture* ktxTex = nullptr;
	KTX_error_code result =
		ktxTexture_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex);

	if (result != KTX_SUCCESS) {
		const char* errorStr = "Unknown";
		switch (result) {
		case KTX_FILE_DATA_ERROR:
			errorStr = "FILE_DATA_ERROR - Invalid file data";
			break;
		case KTX_FILE_OPEN_FAILED:
			errorStr = "FILE_OPEN_FAILED - Could not open file";
			break;
		case KTX_FILE_READ_ERROR:
			errorStr = "FILE_READ_ERROR - Read error";
			break;
		case KTX_INVALID_VALUE:
			errorStr = "INVALID_VALUE";
			break;
		case KTX_NOT_FOUND:
			errorStr = "NOT_FOUND";
			break;
		case KTX_UNKNOWN_FILE_FORMAT:
			errorStr = "UNKNOWN_FILE_FORMAT";
			break;
		case KTX_UNSUPPORTED_TEXTURE_TYPE:
			errorStr = "UNSUPPORTED_TEXTURE_TYPE";
			break;
		default:
			break;
		}
		LogSystem::get().error("Failed to load KTX texture: {} (error: {} - {})", path, static_cast<int>(result),
							   errorStr);
		throw std::runtime_error("Failed to load KTX texture: " + path);
	}

	// Validate cubemap
	if (!ktxTex->isCubemap) {
		ktxTexture_Destroy(ktxTex);
		throw std::runtime_error("KTX texture is not a cubemap: " + path);
	}

	// Check if texture needs transcoding (Basis Universal compression) - only for KTX2
	if (ktxTex->classId == ktxTexture2_c && ktxTexture2_NeedsTranscoding((ktxTexture2*)ktxTex)) {
		LogSystem::get().info("Transcoding Basis Universal texture: {}", path);

		// Get GPU supported format - prefer BC7 for quality, fallback to ASTC or ETC2
		ktx_transcode_fmt_e targetFormat = KTX_TTF_BC7_RGBA;

		result = ktxTexture2_TranscodeBasis((ktxTexture2*)ktxTex, targetFormat, 0);
		if (result != KTX_SUCCESS) {
			ktxTexture_Destroy(ktxTex);
			LogSystem::get().error("Failed to transcode KTX2 texture: {} (error: {})", path, static_cast<int>(result));
			throw std::runtime_error("Failed to transcode KTX2 texture: " + path);
		}
	}

	_width = ktxTex->baseWidth;
	_height = ktxTex->baseHeight;
	_mipLevels = ktxTex->numLevels;

	// Get Vulkan format from KTX
	if (ktxTex->classId == ktxTexture2_c) {
		_format = static_cast<vk::Format>(((ktxTexture2*)ktxTex)->vkFormat);
	} else {
		// For KTX1, get format from glInternalformat
		_format = static_cast<vk::Format>(ktxTexture1_GetVkFormat((ktxTexture1*)ktxTex));
	}

	LogSystem::get().info("Loading cubemap: {} ({}x{}, {} mips, format: {})", path, _width, _height, _mipLevels,
						  vk::to_string(_format));

	// Calculate total image size including all faces and mip levels
	size_t totalSize = ktxTexture_GetDataSize(ktxTex);

	// Create staging buffer
	VulkanBuffer stagingBuffer(_device, totalSize, vk::BufferUsageFlagBits::eTransferSrc,
							   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	stagingBuffer.map();

	// Copy entire texture data to staging buffer
	memcpy(stagingBuffer.getMappedPtr(), ktxTexture_GetData(ktxTex), totalSize);

	// Create VMA image
	vk::ImageCreateInfo imageInfo{.flags = vk::ImageCreateFlagBits::eCubeCompatible,
								  .imageType = vk::ImageType::e2D,
								  .format = _format,
								  .extent = vk::Extent3D{_width, _height, 1},
								  .mipLevels = _mipLevels,
								  .arrayLayers = 6,
								  .samples = vk::SampleCountFlagBits::e1,
								  .tiling = vk::ImageTiling::eOptimal,
								  .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
								  .sharingMode = vk::SharingMode::eExclusive,
								  .initialLayout = vk::ImageLayout::eUndefined};

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	allocInfo.priority = 1.0f;

	VkResult vkResult =
		vmaCreateImage(_device.getVmaAllocator(), reinterpret_cast<const VkImageCreateInfo*>(&imageInfo), &allocInfo,
					   &_image, &_vmaAllocation, nullptr);

	if (vkResult != VK_SUCCESS) {
		ktxTexture_Destroy(ktxTexture(ktxTex));
		throw std::runtime_error("Failed to create VMA image for cubemap");
	}

	// Use shared transfer command pool from VulkanDevice
	auto cmd = _device.getTransferCommandPool().allocateBuffer(true);

	cmd.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

	// Transition to transfer dst
	vk::ImageMemoryBarrier barrier{
		.srcAccessMask = vk::AccessFlagBits::eNone,
		.dstAccessMask = vk::AccessFlagBits::eTransferWrite,
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eTransferDstOptimal,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = _image,
		.subresourceRange =
			{
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = _mipLevels,
				.baseArrayLayer = 0,
				.layerCount = 6,
			},
	};

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
						vk::DependencyFlags(), nullptr, nullptr, barrier);

	// Copy buffer to image for each mip level and face
	std::vector<vk::BufferImageCopy> copyRegions;

	for (uint32_t level = 0; level < _mipLevels; level++) {
		uint32_t mipWidth = std::max(1u, _width >> level);
		uint32_t mipHeight = std::max(1u, _height >> level);

		for (uint32_t face = 0; face < 6; face++) {
			ktx_size_t offset;
			ktxTexture_GetImageOffset(ktxTex, level, 0, face, &offset);

			copyRegions.push_back({
				.bufferOffset = offset,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource =
					{
						.aspectMask = vk::ImageAspectFlagBits::eColor,
						.mipLevel = level,
						.baseArrayLayer = face,
						.layerCount = 1,
					},
				.imageOffset = {0, 0, 0},
				.imageExtent = {mipWidth, mipHeight, 1},
			});
		}
	}

	cmd.copyBufferToImage(stagingBuffer.getBuffer(), _image, vk::ImageLayout::eTransferDstOptimal, copyRegions);

	// Transition to shader read
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
						vk::DependencyFlags(), nullptr, nullptr, barrier);

	cmd.end();

	vk::SubmitInfo submitInfo{
		.commandBufferCount = 1,
		.pCommandBuffers = &(*cmd),
	};

	_device.getGraphicsQueue().submit(submitInfo, nullptr);
	_device.getGraphicsQueue().waitIdle();

	ktxTexture_Destroy(ktxTex);

	LogSystem::get().info("Cubemap loaded successfully: {}", path);
}

void CubemapTexture::createImageView() {
	vk::ImageViewCreateInfo viewInfo{
		.image = _image,
		.viewType = vk::ImageViewType::eCube,
		.format = _format,
		.subresourceRange =
			{
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = _mipLevels,
				.baseArrayLayer = 0,
				.layerCount = 6,
			},
	};

	_imageView = vk::raii::ImageView(*_device, viewInfo);
}

void CubemapTexture::createSampler() {
	auto properties = _device.getPhysicalDevice().getProperties();

	vk::SamplerCreateInfo samplerInfo{
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = vk::SamplerAddressMode::eClampToEdge,
		.addressModeV = vk::SamplerAddressMode::eClampToEdge,
		.addressModeW = vk::SamplerAddressMode::eClampToEdge,
		.mipLodBias = 0.0f,
		.anisotropyEnable = vk::True,
		.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
		.compareEnable = vk::False,
		.compareOp = vk::CompareOp::eAlways,
		.minLod = 0.0f,
		.maxLod = static_cast<float>(_mipLevels),
		.borderColor = vk::BorderColor::eFloatOpaqueBlack,
		.unnormalizedCoordinates = vk::False,
	};

	_sampler = vk::raii::Sampler(*_device, samplerInfo);
}

std::shared_ptr<CubemapTexture> CubemapTexture::createDefault(const VulkanDevice& device) {
	auto cubemap = std::shared_ptr<CubemapTexture>(new CubemapTexture(device));

	// Create a 1x1 black cubemap (6 faces, RGBA8, all zeros)
	uint8_t blackPixels[6 * 4] = {0}; // 6 faces * 4 bytes (RGBA)
	cubemap->createFromData(blackPixels, 1, vk::Format::eR8G8B8A8Unorm);
	cubemap->createImageView();
	cubemap->createSampler();

	LogSystem::get().trace("Created default 1x1 black cubemap");
	return cubemap;
}

void CubemapTexture::createFromData(const uint8_t* data, uint32_t size, vk::Format format) {
	_width = size;
	_height = size;
	_mipLevels = 1;
	_format = format;

	size_t faceSize = size * size * 4; // RGBA8
	size_t totalSize = faceSize * 6;

	// Create staging buffer
	VulkanBuffer stagingBuffer(_device, totalSize, vk::BufferUsageFlagBits::eTransferSrc,
							   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	stagingBuffer.map();

	// Copy face data (repeat same data for all 6 faces)
	uint8_t* dst = static_cast<uint8_t*>(stagingBuffer.getMappedPtr());
	for (int face = 0; face < 6; face++) {
		memcpy(dst + face * faceSize, data, faceSize);
	}

	// Create VMA image
	vk::ImageCreateInfo imageInfo{.flags = vk::ImageCreateFlagBits::eCubeCompatible,
								  .imageType = vk::ImageType::e2D,
								  .format = _format,
								  .extent = vk::Extent3D{_width, _height, 1},
								  .mipLevels = 1,
								  .arrayLayers = 6,
								  .samples = vk::SampleCountFlagBits::e1,
								  .tiling = vk::ImageTiling::eOptimal,
								  .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
								  .sharingMode = vk::SharingMode::eExclusive,
								  .initialLayout = vk::ImageLayout::eUndefined};

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkResult result = vmaCreateImage(_device.getVmaAllocator(), reinterpret_cast<const VkImageCreateInfo*>(&imageInfo),
									 &allocInfo, &_image, &_vmaAllocation, nullptr);

	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create VMA image for default cubemap");
	}

	auto cmd = _device.getTransferCommandPool().allocateBuffer(true);

	cmd.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

	// Transition to transfer dst
	vk::ImageMemoryBarrier barrier{
		.srcAccessMask = vk::AccessFlagBits::eNone,
		.dstAccessMask = vk::AccessFlagBits::eTransferWrite,
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eTransferDstOptimal,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = _image,
		.subresourceRange =
			{
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 6,
			},
	};

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
						vk::DependencyFlags(), nullptr, nullptr, barrier);

	// Copy buffer to image (all 6 faces)
	std::vector<vk::BufferImageCopy> copyRegions;
	for (uint32_t face = 0; face < 6; face++) {
		copyRegions.push_back({
			.bufferOffset = face * faceSize,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource =
				{
					.aspectMask = vk::ImageAspectFlagBits::eColor,
					.mipLevel = 0,
					.baseArrayLayer = face,
					.layerCount = 1,
				},
			.imageOffset = {0, 0, 0},
			.imageExtent = {_width, _height, 1},
		});
	}

	cmd.copyBufferToImage(stagingBuffer.getBuffer(), _image, vk::ImageLayout::eTransferDstOptimal, copyRegions);

	// Transition to shader read
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
						vk::DependencyFlags(), nullptr, nullptr, barrier);

	cmd.end();

	vk::SubmitInfo submitInfo{
		.commandBufferCount = 1,
		.pCommandBuffers = &(*cmd),
	};

	_device.getGraphicsQueue().submit(submitInfo, nullptr);
	_device.getGraphicsQueue().waitIdle();
}

} // namespace Fishy
