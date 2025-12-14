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
class SwapChain {
public:
	SwapChain(vk::raii::Device& device, vk::raii::PhysicalDevice& physicalDevice, vk::raii::SurfaceKHR& surface,
			  int width, int height);
	~SwapChain();

	void create(int width, int height);
	void recreate(int width, int height);

	// 访问接口
	vk::raii::SwapchainKHR& getSwapChain() { return _swapChain; }
	const std::vector<vk::raii::ImageView>& getImageViews() const { return _imageViews; }
	const std::vector<vk::Image>& getImages() const { return _images; }
	vk::Format getFormat() const { return _swapChainSurfaceFormat.format; }
	vk::Extent2D getExtent() const { return _swapChainExtent; }
	uint32_t getImageCount() const { return static_cast<uint32_t>(_imageViews.size()); }

private:
	// 创建与销毁辅助
	void createSwapChain(int width, int height);
	void createImageViews();
	void cleanup();

private:
	vk::raii::Device& _device;
	vk::raii::SurfaceKHR& _surface;
	vk::raii::PhysicalDevice _physicalDevice;

	vk::raii::SwapchainKHR _swapChain = nullptr;
	vk::SurfaceFormatKHR _swapChainSurfaceFormat;
	vk::Extent2D _swapChainExtent;
	std::vector<vk::Image> _images;
	std::vector<vk::raii::ImageView> _imageViews;

	// 窗口尺寸缓存
	int _cachedWidth = 0;
	int _cachedHeight = 0;
};
} // namespace Fishy