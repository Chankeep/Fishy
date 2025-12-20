#include "VulkanDevice.h"
#include <VkBootstrap.h>

namespace Fishy {

VulkanDevice::VulkanDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface)
	: _instance(instance), _surface(surface) {

	vkb::Instance vkb_inst;
	vkb_inst.instance = *instance;
	vkb::PhysicalDeviceSelector selector{vkb_inst};
	auto phys_ret = selector.set_surface(*surface).set_minimum_version(1, 3).select();

	if (!phys_ret) {
		throw std::runtime_error("Failed to select physical device: " + phys_ret.error().message());
	}

	vkb::PhysicalDevice vkb_phys = phys_ret.value();

	// Define required features
	vk::PhysicalDeviceVulkan13Features features13;
	features13.synchronization2 = true;
	features13.dynamicRendering = true;

	vk::PhysicalDeviceVulkan11Features features11;
	features11.shaderDrawParameters = true;

	vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT featuresExt;
	featuresExt.extendedDynamicState = true;

	vkb::DeviceBuilder device_builder{vkb_phys};
	device_builder.add_pNext(&features13).add_pNext(&features11).add_pNext(&featuresExt);
	auto dev_ret = device_builder.build();
	if (!dev_ret) {
		throw std::runtime_error("Failed to build device: " + dev_ret.error().message());
	}

	vkb::Device vkb_device = dev_ret.value();

	// Initialize volk for this device
	volkLoadDevice(vkb_device.device);

	_physicalDevice = vk::raii::PhysicalDevice(instance, vkb_device.physical_device);

	// Create RAII Device wrapper around existing device
	// Note: We need to be careful here. vk::raii::Device usually destroys the device on destruction.
	// Since vkb created it, we pass it to RAII and RAII will destroy it.
	_device = vk::raii::Device(_physicalDevice, vkb_device.device);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(*_device);

	// Retrieve queues
	auto graphics_queue_idx_ret = vkb_device.get_queue_index(vkb::QueueType::graphics);
	auto present_queue_idx_ret = vkb_device.get_queue_index(vkb::QueueType::present);

	if (!graphics_queue_idx_ret || !present_queue_idx_ret) {
		throw std::runtime_error("Failed to get queues after device creation");
	}

	_graphicsQueueFamily = graphics_queue_idx_ret.value();

	_graphicsQueue = vk::raii::Queue(_device, _graphicsQueueFamily, 0);
	_presentQueue = vk::raii::Queue(_device, present_queue_idx_ret.value(), 0);
}

VulkanDevice::~VulkanDevice() {}

} // namespace Fishy