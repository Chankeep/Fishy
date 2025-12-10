#include "SwapChain.h"

namespace Fishy {

// ============ 辅助函数（成员函数） ============

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
	// 优先选择 SRGB + Linear colorspace（推荐）
	for (const auto &format : availableFormats) {
		if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			return format;
		}
	}

	// 如果没有最优选择，返回第一个（通常也不错）
	return availableFormats.front();
}

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
	// 优先选择 Mailbox（低延迟、三缓冲）
	for (const auto &mode : availablePresentModes) {
		if (mode == vk::PresentModeKHR::eMailbox) {
			return mode;
		}
	}

	// fallback 到 FIFO（始终可用）
	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities, int width, int height) {
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

uint32_t chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR &capabilities) {
	// 通常选择 minImageCount + 1（三缓冲）
	uint32_t imageCount = capabilities.minImageCount + 1;

	// 如果有最大限制且超过，使用最大值
	if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
		imageCount = capabilities.maxImageCount;
	}

	return imageCount;
}

SwapChain::SwapChain(vk::raii::Device &device, vk::raii::PhysicalDevice &physicalDevice, vk::raii::SurfaceKHR &surface,
					 int width, int height)
	: _device(device), _physicalDevice(physicalDevice), _surface(surface), _cachedWidth(width), _cachedHeight(height) {
	create(width, height);
}

SwapChain::~SwapChain() { cleanup(); }

void SwapChain::create(int width, int height) {
	_cachedWidth = width;
	_cachedHeight = height;
	createSwapChain(width, height);
	createImageViews();
}

void SwapChain::recreate(int width, int height) {
	// 如果尺寸未改变，则不需要重建
	if (width == _cachedWidth && height == _cachedHeight) {
		return;
	}

	// 等待当前操作完成
	_device.waitIdle();

	// 清理旧资源
	cleanup();

	// 创建新的 swapchain
	create(width, height);
}

void SwapChain::createSwapChain(int width, int height) {
	auto surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR(*_surface);
	_swapChainExtent = chooseSwapExtent(surfaceCapabilities, width, height);
	_swapChainSurfaceFormat = chooseSwapSurfaceFormat(_physicalDevice.getSurfaceFormatsKHR(*_surface));

	vk::SwapchainCreateInfoKHR swapChainCreateInfo{
		.surface = *_surface,
		.minImageCount = chooseSwapMinImageCount(surfaceCapabilities),
		.imageFormat = _swapChainSurfaceFormat.format,
		.imageColorSpace = _swapChainSurfaceFormat.colorSpace,
		.imageExtent = _swapChainExtent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.preTransform = surfaceCapabilities.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = chooseSwapPresentMode(_physicalDevice.getSurfacePresentModesKHR(*_surface)),
		.clipped = true};

	_swapChain = vk::raii::SwapchainKHR(_device, swapChainCreateInfo);
	_images = _swapChain.getImages();
}

void SwapChain::createImageViews() {
	_imageViews.clear();

	vk::ImageViewCreateInfo imageViewCreateInfo{.viewType = vk::ImageViewType::e2D,
												.format = _swapChainSurfaceFormat.format,
												.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
	for (auto image : _images) {
		imageViewCreateInfo.image = image;
		_imageViews.emplace_back(_device, imageViewCreateInfo);
	}
}

void SwapChain::cleanup() {
	// 销毁 image views（swapchain 销毁时会自动释放 images）
	_imageViews.clear();
	// swapchain 由 vk::raii::SwapchainKHR 自动管理，赋值 nullptr 即可销毁
	_swapChain = nullptr;
}

} // namespace Fishy
