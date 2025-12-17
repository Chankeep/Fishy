#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS // 移除Vulkan.hpp的构造函数

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <limits>
#include <vector>

namespace Fishy {

class VulkanDevice;

class SwapChain {
public:
	SwapChain(const VulkanDevice& device, const vk::raii::SurfaceKHR& surface, int width, int height);
	~SwapChain() = default;

	// 禁止拷贝，允许移动 (因为内部持有引用，移动也比较麻烦，通常建议直接禁用拷贝)
	SwapChain(const SwapChain&) = delete;
	SwapChain& operator=(const SwapChain&) = delete;

	void recreate(int width, int height);

	// 访问接口
	const vk::raii::SwapchainKHR& get() const { return _swapChain; }
	const vk::raii::SwapchainKHR& operator*() const { return _swapChain; }
	const std::vector<vk::raii::ImageView>& getImageViews() const { return _imageViews; }
	const std::vector<vk::Image>& getImages() const { return _images; }
	vk::Format getFormat() const { return _swapChainSurfaceFormat.format; }
	vk::Extent2D getExtent() const { return _swapChainExtent; }
	uint32_t getImageCount() const { return static_cast<uint32_t>(_imageViews.size()); }

private:
	// 创建与销毁辅助
	void createSwapChain(int width, int height);
	void createImageViews();

private:
	const VulkanDevice& _device;
	const vk::raii::SurfaceKHR& _surface;

	vk::raii::SwapchainKHR _swapChain = nullptr;
	vk::SurfaceFormatKHR _swapChainSurfaceFormat;
	vk::Extent2D _swapChainExtent;
	std::vector<vk::Image> _images;
	std::vector<vk::raii::ImageView> _imageViews;

};
} // namespace Fishy