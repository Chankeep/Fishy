#include "RenderTexture.h"

#include "core/VulkanUtils.h"

namespace Fishy {

RenderTexture::RenderTexture(VulkanDevice& device, uint32_t width, uint32_t height, vk::Format format,
							 vk::ImageUsageFlags usage)
	: _device(device), _format(format), _usage(usage) {
	createResources(width, height);
}

RenderTexture::~RenderTexture() { destroyResources(); }

vk::ImageView RenderTexture::getImageView() const {
	return _image ? *_image->getView() : vk::ImageView{nullptr};
}

vk::Image RenderTexture::getImage() const {
	return _image ? _image->getImage() : vk::Image{nullptr};
}

bool RenderTexture::isDepthFormat() const {
	return _format == vk::Format::eD16Unorm || _format == vk::Format::eD32Sfloat ||
		   _format == vk::Format::eD16UnormS8Uint || _format == vk::Format::eD24UnormS8Uint ||
		   _format == vk::Format::eD32SfloatS8Uint;
}

vk::ImageAspectFlags RenderTexture::getAspectFlags() const {
	if (!isDepthFormat()) {
		return vk::ImageAspectFlagBits::eColor;
	}

	vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eDepth;
	if (_format == vk::Format::eD32SfloatS8Uint || _format == vk::Format::eD24UnormS8Uint ||
		_format == vk::Format::eD16UnormS8Uint) {
		aspect |= vk::ImageAspectFlagBits::eStencil;
	}
	return aspect;
}

void RenderTexture::recreate(uint32_t width, uint32_t height) {
	destroyResources();
	createResources(width, height);
}

void RenderTexture::createResources(uint32_t width, uint32_t height) {
	_extent = {width, height};

	FISHY_LOG_TRACE("Creating RenderTexture: {}x{} format:{}", width, height, vk::to_string(_format));

	// Create Image
	vk::ImageCreateInfo imageInfo{.imageType = vk::ImageType::e2D,
								  .format = _format,
								  .extent = {width, height, 1},
								  .mipLevels = 1,
								  .arrayLayers = 1,
								  .samples = vk::SampleCountFlagBits::e1,
								  .tiling = vk::ImageTiling::eOptimal,
								  .usage = _usage,
								  .sharingMode = vk::SharingMode::eExclusive,
								  .initialLayout = vk::ImageLayout::eUndefined};

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

	_image = std::make_unique<VulkanImage>(_device.getVmaAllocator(), imageInfo, allocInfo);

#ifndef NDEBUG
	std::string prefix = isDepthFormat() ? "DepthRT_" : "ColorRT_";
	std::string debugName = prefix + std::to_string(width) + "x" + std::to_string(height);
	VulkanUtils::setDebugName(_device, _image->getImage(), debugName.c_str());
#endif

	// Create View
	auto aspect = getAspectFlags();
	vk::ImageViewCreateInfo viewInfo{.viewType = vk::ImageViewType::e2D,
									 .format = _format,
									 .subresourceRange = {.aspectMask = aspect,
														  .baseMipLevel = 0,
														  .levelCount = 1,
														  .baseArrayLayer = 0,
														  .layerCount = 1}};

	_image->createView(*_device, viewInfo);

#ifndef NDEBUG
	VulkanUtils::setDebugName(_device, *_image->getView(), vk::ObjectType::eImageView,
							  (debugName + "_View").c_str());
#endif

	// Layout transition to appropriate initial layout
	auto targetLayout = isDepthFormat() ? vk::ImageLayout::eDepthStencilAttachmentOptimal
										: vk::ImageLayout::eColorAttachmentOptimal;

	auto dstAccess = isDepthFormat()
						 ? (vk::AccessFlagBits2::eDepthStencilAttachmentRead |
							vk::AccessFlagBits2::eDepthStencilAttachmentWrite)
						 : (vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite);

	auto dstStage =
		isDepthFormat()
			? (vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests)
			: vk::PipelineStageFlagBits2::eColorAttachmentOutput;

	VulkanUtils::executeImmediate(_device, [&](auto& cmd) {
		VulkanUtils::transitionImage(*cmd, *_image, vk::ImageLayout::eUndefined, targetLayout,
									 vk::AccessFlagBits2::eNone, dstAccess,
									 vk::PipelineStageFlagBits2::eTopOfPipe, dstStage, aspect);
	});

	FISHY_LOG_TRACE("RenderTexture created and transitioned to {}", vk::to_string(targetLayout));
}

void RenderTexture::destroyResources() {
	_image.reset();
	_extent = {0, 0};
}

} // namespace Fishy