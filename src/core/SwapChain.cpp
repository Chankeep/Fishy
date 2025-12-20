#include "SwapChain.h"
#include "VulkanDevice.h"

namespace Fishy {

// ============ Helper Functions (Member Functions) ============

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
	// Prefer SRGB + Linear colorspace (Recommended)
	for (const auto& format : availableFormats) {
		if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			return format;
		}
	}

	// If no optimal choice, return the first one (usually good enough)
	return availableFormats.front();
}

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
	// Prefer Mailbox (Low latency, Triple Buffering)
	for (const auto& mode : availablePresentModes) {
		if (mode == vk::PresentModeKHR::eMailbox) {
			return mode;
		}
	}

	// fallback to FIFO (Always available)
	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, int width, int height) {
	// If currentExtent is not uint32_t max, use the current value
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}

	// Otherwise, clamp width and height to min/max bounds
	vk::Extent2D extent{.width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)};

	extent.width =
		std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, extent.width));
	extent.height =
		std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, extent.height));

	return extent;
}

uint32_t chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR& capabilities) {
	// Usually choose minImageCount + 1 (Triple Buffering)
	uint32_t imageCount = capabilities.minImageCount + 1;

	// If there is a max limit and we exceed it, clamp to max
	if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
		imageCount = capabilities.maxImageCount;
	}

	return imageCount;
}

SwapChain::SwapChain(const VulkanDevice& device, const vk::raii::SurfaceKHR& surface, int width, int height)
	: _device(device), _surface(surface) {
	createSwapChain(width, height);
}

void SwapChain::recreate(int width, int height) {
	// 1. Handle minimization (width or height is 0)
	if (width == 0 || height == 0) {
		return;
	}

	// We let createSwapChain handle the "old -> new" logic or overwrite member variables directly.
	// vk::raii assignment operator will automatically destroy the old object.

	createSwapChain(width, height);
}

void SwapChain::createSwapChain(int width, int height) {
	auto surfaceCapabilities = _device.getPhysicalDevice().getSurfaceCapabilitiesKHR(*_surface);
	_swapChainExtent = chooseSwapExtent(surfaceCapabilities, width, height);
	_swapChainSurfaceFormat = chooseSwapSurfaceFormat(_device.getPhysicalDevice().getSurfaceFormatsKHR(*_surface));

	vk::SharingMode sharingMode = vk::SharingMode::eExclusive;
	std::vector<uint32_t> queueFamilyIndices;

	uint32_t graphicsFamily = _device.getGraphicsQueueFamilyIndex();

	// Assume VulkanDevice provides getPresentQueueFamilyIndex(), if not, add it
	// uint32_t presentFamily = _device.getPresentQueueFamilyIndex();
	uint32_t presentFamily =
		graphicsFamily; // Temporary assumption: same queue family. Please modify based on actual Device class.

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

} // namespace Fishy
