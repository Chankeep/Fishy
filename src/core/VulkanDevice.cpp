#include "VulkanDevice.h"

namespace Fishy {

VulkanDevice::VulkanDevice(const vk::raii::Instance& instance, const vk::raii::SurfaceKHR& surface)
	: _instance(instance), _surface(surface) {
	pickPhysicalDevice();
	createLogicalDevice();
}

VulkanDevice::~VulkanDevice() {}

// Define required device extensions
std::vector<const char*> requiredDeviceExtension = {vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
													vk::KHRSynchronization2ExtensionName,
													vk::KHRCreateRenderpass2ExtensionName};

void VulkanDevice::pickPhysicalDevice() {
	std::vector<vk::raii::PhysicalDevice> devices = _instance.enumeratePhysicalDevices();
	const auto devIter = std::ranges::find_if(devices, [&](auto const& device) {
		// Check if the device supports the Vulkan 1.3 API version
		bool supportsVulkan1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

		// Check if any of the queue families support graphics operations
		auto queueFamilies = device.getQueueFamilyProperties();
		bool supportsGraphics = std::ranges::any_of(
			queueFamilies, [](auto const& qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

		// Check if all required device extensions are available
		auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
		bool supportsAllRequiredExtensions = std::ranges::all_of(
			requiredDeviceExtension, [&availableDeviceExtensions](auto const& requiredDeviceExtension) {
				return std::ranges::any_of(
					availableDeviceExtensions, [requiredDeviceExtension](auto const& availableDeviceExtension) {
						return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
					});
			});

		auto features = device.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
													 vk::PhysicalDeviceVulkan13Features,
													 vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
		bool supportsRequiredFeatures =
			features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
			features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
			features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
			features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

		return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
	});
	if (devIter != devices.end()) {
		_physicalDevice = *devIter;
	} else {
		throw std::runtime_error("failed to find a suitable GPU!");
	}
}
void VulkanDevice::createLogicalDevice() {
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = _physicalDevice.getQueueFamilyProperties();

	// get the first index into queueFamilyProperties which supports both
	// graphics and present
	for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++) {
		if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
			_physicalDevice.getSurfaceSupportKHR(qfpIndex, *_surface)) {
			// found a queue family that supports both graphics and present
			_graphicsQueueFamily = qfpIndex;
			break;
		}
	}
	if (_graphicsQueueFamily == ~0) {
		throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
	}

	// query for Vulkan 1.3 features
	vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
					   vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		featureChain = {
			{},
			// vk::PhysicalDeviceFeatures2
			{.shaderDrawParameters = true},
			// vk::PhysicalDeviceVulkan11Features
			{.synchronization2 = true, .dynamicRendering = true},
			// vk::PhysicalDeviceVulkan13Features
			{.extendedDynamicState = true} // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
		};

	// create a Device
	float queuePriority = 0.0f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
		.queueFamilyIndex = _graphicsQueueFamily, .queueCount = 1, .pQueuePriorities = &queuePriority};
	vk::DeviceCreateInfo deviceCreateInfo{.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
										  .queueCreateInfoCount = 1,
										  .pQueueCreateInfos = &deviceQueueCreateInfo,
										  .enabledExtensionCount =
											  static_cast<uint32_t>(requiredDeviceExtension.size()),
										  .ppEnabledExtensionNames = requiredDeviceExtension.data()};

	_device = vk::raii::Device(_physicalDevice, deviceCreateInfo);
	_graphicsQueue = vk::raii::Queue(_device, _graphicsQueueFamily, 0);
	_presentQueue = vk::raii::Queue(_device, _graphicsQueueFamily, 0);
}
} // namespace Fishy