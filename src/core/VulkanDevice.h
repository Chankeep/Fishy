#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS // 移除Vulkan.hpp的构造函数

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace Fishy {

class VulkanContext;

class VulkanDevice {
public:
	VulkanDevice(vk::raii::Instance& instance, vk::raii::SurfaceKHR& surface);
	~VulkanDevice();

	vk::raii::Device& getDevice() { return _device; }
	vk::raii::Queue& getGraphicsQueue() { return _graphicsQueue; }
	vk::raii::Queue& getPresentQueue() { return _presentQueue; }
	vk::raii::PhysicalDevice& getPhysicalDevice() { return _physicalDevice; }
	uint32_t getGraphicsQueueFamilyIndex() const { return _graphicsQueueFamily; }

private:
	vk::raii::Instance& _instance;
	vk::raii::SurfaceKHR& _surface;
	vk::raii::Device _device = nullptr;
	vk::raii::Queue _graphicsQueue = nullptr;
	vk::raii::Queue _presentQueue = nullptr;
	vk::raii::PhysicalDevice _physicalDevice = nullptr;

	uint32_t _graphicsQueueFamily;

	void pickPhysicalDevice();
	void createLogicalDevice();
};
} // namespace Fishy