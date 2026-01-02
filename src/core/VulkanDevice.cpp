#include "vulkan/vulkan.hpp"
#include <stdexcept>

// VMA_IMPLEMENTATION must be defined in exactly one .cpp file before including vk_mem_alloc.h
#define VMA_IMPLEMENTATION
#include "VulkanDevice.h"
#include <VkBootstrap.h>

namespace Fishy {

VulkanDevice::VulkanDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface)
	: _instance(instance), _surface(surface) {

	auto physicalDevice = selectPhysicalDevice();
	createLogicalDevice(physicalDevice);
	initQueues();
	initVmaAllocator();

	// Create transfer command pool for texture uploads
	_transferCommandPool = std::make_unique<CommandPool>(*this, _graphicsQueueFamily);
}

VulkanDevice::~VulkanDevice() {
	_transferCommandPool.reset();
	if (_vmaAllocator != VK_NULL_HANDLE) {
		vmaDestroyAllocator(_vmaAllocator);
	}
}

vkb::PhysicalDevice VulkanDevice::selectPhysicalDevice() {
	LogSystem::get().info("Selecting physical device...");

	vkb::Instance vkb_inst;
	vkb_inst.instance = *_instance;
	vkb::PhysicalDeviceSelector selector{vkb_inst};
	auto phys_ret = selector.set_surface(*_surface).set_minimum_version(1, 3).select();

	if (!phys_ret) {
		LogSystem::get().error("Failed to select physical device: {}", phys_ret.error().message());
		throw std::runtime_error("Failed to select physical device: " + phys_ret.error().message());
	}

	vkb::PhysicalDevice vkb_phys = phys_ret.value();

	// Log selected device info
	LogSystem::get().info("Selected physical device: {}", vkb_phys.properties.deviceName);
	LogSystem::get().info("  Driver Version: {}", vkb_phys.properties.driverVersion);
	LogSystem::get().info("  API Version: {}.{}.{}", VK_VERSION_MAJOR(vkb_phys.properties.apiVersion),
						  VK_VERSION_MINOR(vkb_phys.properties.apiVersion),
						  VK_VERSION_PATCH(vkb_phys.properties.apiVersion));

	return vkb_phys;
}

void VulkanDevice::createLogicalDevice(const vkb::PhysicalDevice& physicalDevice) {
	// Vulkan 1.3 features
	vk::PhysicalDeviceVulkan13Features features13;
	features13.synchronization2 = true;
	features13.dynamicRendering = true;

	// Vulkan 1.1 features
	vk::PhysicalDeviceVulkan11Features features11;
	features11.shaderDrawParameters = true;

	// Core features
	vk::PhysicalDeviceFeatures2 features2;
	features2.features.samplerAnisotropy = true;

	// Extended dynamic state
	vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT featuresExt;
	featuresExt.extendedDynamicState = true;

	// Descriptor indexing for bindless resources
	vk::PhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures;
	// Bindless textures
	descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = true;
	descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = true;
	// Bindless storage buffers (for dynamic instance data pools)
	descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = true;
	// Common features
	descriptorIndexingFeatures.runtimeDescriptorArray = true;
	descriptorIndexingFeatures.descriptorBindingPartiallyBound = true;
	descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = true;

	LogSystem::get().info("[Bindless] Runtime streaming features enabled:");
	LogSystem::get().info("  - Sampled image array + update after bind");
	LogSystem::get().info("  - Storage buffer array + update after bind");
	LogSystem::get().info("  - Runtime descriptor array, partially bound, variable count");

	// Build logical device
	vkb::DeviceBuilder device_builder{physicalDevice};
	device_builder.add_pNext(&features13)
		.add_pNext(&features11)
		.add_pNext(&features2)
		.add_pNext(&featuresExt)
		.add_pNext(&descriptorIndexingFeatures);

	auto dev_ret = device_builder.build();
	if (!dev_ret) {
		LogSystem::get().error("Failed to build device: {}", dev_ret.error().message());
		throw std::runtime_error("Failed to build device: " + dev_ret.error().message());
	}

	_vkbDevice = dev_ret.value();
	LogSystem::get().info("Logical device created successfully");

	// Initialize volk for this device
	volkLoadDevice(_vkbDevice.device);

	// Create RAII wrappers
	_physicalDevice = vk::raii::PhysicalDevice(_instance, _vkbDevice.physical_device);
	_device = vk::raii::Device(_physicalDevice, _vkbDevice.device);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(*_device);
}

void VulkanDevice::initQueues() {
	auto graphics_queue_idx_ret = _vkbDevice.get_queue_index(vkb::QueueType::graphics);
	auto present_queue_idx_ret = _vkbDevice.get_queue_index(vkb::QueueType::present);

	if (!graphics_queue_idx_ret || !present_queue_idx_ret) {
		LogSystem::get().error("Failed to get queues after device creation");
		throw std::runtime_error("Failed to get queues after device creation");
	}

	_graphicsQueueFamily = graphics_queue_idx_ret.value();

	_graphicsQueue = vk::raii::Queue(_device, _graphicsQueueFamily, 0);
	_presentQueue = vk::raii::Queue(_device, present_queue_idx_ret.value(), 0);

	LogSystem::get().info("Graphics queue family: {}", _graphicsQueueFamily);
	LogSystem::get().info("Present queue family: {}", present_queue_idx_ret.value());
}

void VulkanDevice::initVmaAllocator() {
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = *_physicalDevice;
	allocatorInfo.device = *_device;
	allocatorInfo.instance = *_instance;
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	allocatorInfo.pVulkanFunctions = &vulkanFunctions;

	auto vma_res = vmaCreateAllocator(&allocatorInfo, &_vmaAllocator);

	if (vma_res != VK_SUCCESS) {
		LogSystem::get().error("Failed to create VMA allocator!");
		throw std::runtime_error("Failed to create VMA allocator");
	}
	LogSystem::get().info("VMA allocator created successfully");
}

uint32_t VulkanDevice::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const {
	vk::PhysicalDeviceMemoryProperties memProperties = _physicalDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

} // namespace Fishy