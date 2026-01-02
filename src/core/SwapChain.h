#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <vector>

namespace Fishy {

class VulkanDevice;

class SwapChain {
public:
	SwapChain(const VulkanDevice& device, const vk::raii::SurfaceKHR& surface, int width, int height);
	~SwapChain() = default;

	// Disable copying, allow moving (Since it holds references, moving is tricky, better to disable copy)
	SwapChain(const SwapChain&) = delete;
	SwapChain& operator=(const SwapChain&) = delete;

	void recreate(int width, int height);

	// Accessors
	[[nodiscard]] const vk::raii::SwapchainKHR& get() const { return _swapChain; }
	[[nodiscard]] const vk::raii::SwapchainKHR& operator*() const { return _swapChain; }
	[[nodiscard]] const std::vector<vk::Image>& getImages() const { return _images; }
	[[nodiscard]] vk::Format getFormat() const { return _swapChainSurfaceFormat.format; }
	[[nodiscard]] vk::Extent2D getExtent() const { return _swapChainExtent; }
	[[nodiscard]] uint32_t getImageCount() const { return static_cast<uint32_t>(_images.size()); }

private:
	// Creation and Destruction helpers
	void createSwapChain(int width, int height);

	const VulkanDevice& _device;
	const vk::raii::SurfaceKHR& _surface;

	vk::raii::SwapchainKHR _swapChain = nullptr;
	vk::SurfaceFormatKHR _swapChainSurfaceFormat;
	vk::Extent2D _swapChainExtent;
	std::vector<vk::Image> _images;
};
} // namespace Fishy