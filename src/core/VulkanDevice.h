#pragma once

#include "CommandPool.h"
#include "vk_mem_alloc.h"
#include <VkBootstrap.h>
#include <volk.h>
#include <vulkan/vulkan_core.h>

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

	VulkanDevice(const VulkanDevice&) = delete;
	VulkanDevice& operator=(const VulkanDevice&) = delete;
	VulkanDevice(VulkanDevice&&) = delete;
	VulkanDevice& operator=(VulkanDevice&&) = delete;

	[[nodiscard]] const vk::raii::Device& operator*() const { return _device; }
	[[nodiscard]] const vk::raii::Device* operator->() const { return &_device; }

	[[nodiscard]] const vk::raii::Queue& getGraphicsQueue() const { return _graphicsQueue; }
	[[nodiscard]] const vk::raii::Queue& getPresentQueue() const { return _presentQueue; }
	[[nodiscard]] const vk::raii::PhysicalDevice& getPhysicalDevice() const { return _physicalDevice; }
	[[nodiscard]] uint32_t getGraphicsQueueFamilyIndex() const { return _graphicsQueueFamily; }
	[[nodiscard]] const vkb::Device& getVkbDevice() const { return _vkbDevice; }
	[[nodiscard]] VmaAllocator getVmaAllocator() const { return _vmaAllocator; }

	// Transfer command pool for texture uploads and other transfer operations
	[[nodiscard]] CommandPool& getTransferCommandPool() { return *_transferCommandPool; }
	[[nodiscard]] const CommandPool& getTransferCommandPool() const { return *_transferCommandPool; }

	[[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;

private:
	// Initialization helpers (called from constructor)
	vkb::PhysicalDevice selectPhysicalDevice();
	void createLogicalDevice(const vkb::PhysicalDevice& physicalDevice);
	void initQueues();
	void initVmaAllocator();

	// These references must outlive this VulkanDevice instance.
	// VulkanContext is responsible for ensuring correct destruction order.
	const vk::raii::Instance& _instance;
	const vk::raii::SurfaceKHR& _surface;

	vk::raii::Device _device = nullptr;
	vk::raii::Queue _graphicsQueue = nullptr;
	vk::raii::Queue _presentQueue = nullptr;
	vk::raii::PhysicalDevice _physicalDevice = nullptr;

	vkb::Device _vkbDevice;
	VmaAllocator _vmaAllocator = VK_NULL_HANDLE;
	std::unique_ptr<CommandPool> _transferCommandPool;

	uint32_t _graphicsQueueFamily = 0;
};
} // namespace Fishy