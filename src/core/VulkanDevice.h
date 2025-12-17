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
	VulkanDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface);
	~VulkanDevice();

	const vk::raii::Device& operator*() const { return _device; }
	const vk::raii::Device* operator->() const { return &_device; }

	const vk::raii::Queue& getGraphicsQueue() const { return _graphicsQueue; }
	const vk::raii::Queue& getPresentQueue() const { return _presentQueue; }
	const vk::raii::PhysicalDevice& getPhysicalDevice() const { return _physicalDevice; }
	const uint32_t getGraphicsQueueFamilyIndex() const { return _graphicsQueueFamily; }

private:
	const vk::raii::Instance& _instance;
	const vk::raii::SurfaceKHR& _surface;
	vk::raii::Device _device = nullptr;
	vk::raii::Queue _graphicsQueue = nullptr;
	vk::raii::Queue _presentQueue = nullptr;
	vk::raii::PhysicalDevice _physicalDevice = nullptr;

	uint32_t _graphicsQueueFamily;

	void pickPhysicalDevice();
	void createLogicalDevice();
};
} // namespace Fishy