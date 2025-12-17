#include "SwapChain.h"
#include "VulkanDevice.h"

namespace Fishy {

// ============ 辅助函数（成员函数） ============

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
	// 优先选择 SRGB + Linear colorspace（推荐）
	for (const auto& format : availableFormats) {
		if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			return format;
		}
	}

	// 如果没有最优选择，返回第一个（通常也不错）
	return availableFormats.front();
}

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
	// 优先选择 Mailbox（低延迟、三缓冲）
	for (const auto& mode : availablePresentModes) {
		if (mode == vk::PresentModeKHR::eMailbox) {
			return mode;
		}
	}

	// fallback 到 FIFO（始终可用）
	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, int width, int height) {
	// 如果 currentExtent 不是 uint32_t 最大值，使用当前值
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}

	// 否则，钳制宽高到 min/max bounds
	vk::Extent2D extent{.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)};

	extent.width =
		std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
	extent.height =
		std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent.height));

	return extent;
}

uint32_t chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& capabilities) {
	// 通常选择 minImageCount + 1（三缓冲）
	uint32_t imageCount = capabilities.minImageCount + 1;

	// 如果有最大限制且超过，使用最大值
	if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
		imageCount = capabilities.maxImageCount;
	}

	return imageCount;
}

SwapChain::SwapChain(const VulkanDevice& device, const vk::raii::SurfaceKHR& surface, int width, int height)
	: _device(device), _surface(surface) {
	createSwapChain(width, height);
	createImageViews();
}

void SwapChain::recreate(int width, int height) {
	// 1. 处理最小化 (宽高为0)
	if (width == 0 || height == 0) {
		return;
	}
	
	// 我们让 createSwapChain 内部处理“旧换新”逻辑，或者直接覆盖成员变量。
	// vk::raii 的赋值操作符会自动销毁旧对象。

	createSwapChain(width, height);
	createImageViews();
}

void SwapChain::createSwapChain(int width, int height) {
	auto surfaceCapabilities = _device.getPhysicalDevice().getSurfaceCapabilitiesKHR(*_surface);
	_swapChainExtent = chooseSwapExtent(surfaceCapabilities, width, height);
	_swapChainSurfaceFormat = chooseSwapSurfaceFormat(_device.getPhysicalDevice().getSurfaceFormatsKHR(*_surface));

	vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
	std::vector<uint32_t> queueFamilyIndices;

	uint32_t graphicsFamily = _device.getGraphicsQueueFamilyIndex();
	// 假设 VulkanDevice 提供了 getPresentQueueFamilyIndex()，如果没有，需要加上
	// uint32_t presentFamily = _device.getPresentQueueFamilyIndex();
	uint32_t presentFamily = graphicsFamily; // 临时假设相同，请根据实际 Device 类修改

	if (graphicsFamily != presentFamily) {
		sharingMode = vk::SharingMode::eConcurrent;
		queueFamilyIndices = {graphicsFamily, presentFamily};
	}
	vk::SwapchainCreateInfoKHR swapChainCreateInfo{
		.surface = *_surface,
		.minImageCount = chooseSwapMinImageCount(surfaceCapabilities),
		.imageFormat = _swapChainSurfaceFormat.format,
		.imageColorSpace = _swapChainSurfaceFormat.colorSpace,
		.imageExtent = _swapChainExtent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size()),
		.pQueueFamilyIndices = queueFamilyIndices.data(),
		.preTransform = surfaceCapabilities.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = chooseSwapPresentMode(_device.getPhysicalDevice().getSurfacePresentModesKHR(*_surface)),
		.clipped = true,
		.oldSwapchain = *_swapChain};

	_swapChain = vk::raii::SwapchainKHR(*_device, swapChainCreateInfo);
	_images = _swapChain.getImages();
}

void SwapChain::createImageViews() {
	_imageViews.clear();
	_imageViews.reserve(_images.size());

	vk::ImageViewCreateInfo imageViewCreateInfo{.viewType = vk::ImageViewType::e2D,
												.format = _swapChainSurfaceFormat.format,
												.subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
																	 .baseMipLevel = 0,
																	 .levelCount = 1,
																	 .baseArrayLayer = 0,
																	 .layerCount = 1}};
	for (auto image : _images) {
		imageViewCreateInfo.image = image;
		_imageViews.emplace_back(*_device, imageViewCreateInfo);
	}
}

} // namespace Fishy
