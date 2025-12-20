#pragma once

#include <volk.h>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace Fishy {

class VulkanContext;

/**
 * @brief Manages the logical Vulkan Device and Queues.
 *
 * Handles physical device selection, logical device creation, and
 * retrieval of graphics and present queues.
 */
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
};
} // namespace Fishy