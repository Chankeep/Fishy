#include "SwapChain.h"
#include "VulkanDevice.h"
#include <vulkan/vulkan_core.h>

namespace Fishy {

SwapChain::SwapChain(const VulkanDevice& device, const vk::raii::SurfaceKHR& surface, int width, int height)
	: _device(device), _surface(surface) {
	createSwapChain(width, height);
}

void SwapChain::recreate(int width, int height) {
	// 1. Handle minimization (width or height is 0)
	if (width == 0 || height == 0) {
		return;
	}

	createSwapChain(width, height);
}

void SwapChain::createSwapChain(int width, int height) {
	vkb::SwapchainBuilder swapchainBuilder{_device.getVkbDevice()};

	// Note: We currently do not pass the old swapchain to vkb because
	// vk::raii::SwapchainKHR owns it and will destroy it.
	// vkb also attempts to destroy the old swapchain by default.
	// To avoid double-free and complexity with releasing RAII ownership, we accept creating a fresh swapchain.

	auto swap_ret =
		swapchainBuilder.set_desired_extent(width, height)
			.set_desired_format(VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
			.add_fallback_format(VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
			// Use FIFO (vsync) as default for lower GPU usage.
			// Falls back to MAILBOX (triple buffer) if FIFO is not available.
			.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
			.add_fallback_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
			.set_composite_alpha_flags(VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
			.set_old_swapchain(*_swapChain)
			.build();

	if (!swap_ret) {
		FISHY_LOG_ERROR("Failed to create swapchain: {}", swap_ret.error().message());
		throw std::runtime_error("Failed to create swapchain: " + swap_ret.error().message());
	}

	vkb::Swapchain vkbSwapchain = swap_ret.value();

	FISHY_LOG_INFO("Swapchain created: {}x{} format:{} images:{}", vkbSwapchain.extent.width,
						  vkbSwapchain.extent.height, vk::to_string(static_cast<vk::Format>(vkbSwapchain.image_format)),
						  vkbSwapchain.image_count);

	// Wrap in RAII
	// The previous swapchain will be destroyed automatically when reassigned.
	_swapChain = vk::raii::SwapchainKHR(*_device, vkbSwapchain.swapchain);

	_swapChainExtent = vkbSwapchain.extent;
	_swapChainSurfaceFormat = vk::SurfaceFormatKHR(static_cast<vk::Format>(vkbSwapchain.image_format),
												   static_cast<vk::ColorSpaceKHR>(vkbSwapchain.color_space));

	_images = _swapChain.getImages();
}

} // namespace Fishy
