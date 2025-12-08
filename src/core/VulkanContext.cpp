#include "VulkanContext.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

namespace Fishy {

// Define validation layers
const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

// Define required device extensions
std::vector<const char *> requiredDeviceExtension = {vk::KHRSwapchainExtensionName, vk::KHRSpirv14ExtensionName,
													 vk::KHRSynchronization2ExtensionName,
													 vk::KHRCreateRenderpass2ExtensionName};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

void VulkanContext::Init(GLFWwindow *window) {
	// Initialize RAII Context (loads Vulkan library)
	_context = vk::raii::Context();

	createInstance();
	setupDebugMessenger();
	createSurface(window);
	pickPhysicalDevice();
	createLogicalDevice();
}

void VulkanContext::Cleanup() {
	// RAII handles handle destruction automatically.
	// Destroyed in reverse order of declaration in header.
}

std::vector<const char *> getRequiredExtensions() {
	uint32_t glfwExtensionCount = 0;
	const char **glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
													  vk::DebugUtilsMessageTypeFlagsEXT type,
													  const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
													  void *) {
	if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
		severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
		std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
	}

	return vk::False;
}

void VulkanContext::createInstance() {
	vk::ApplicationInfo appInfo{
		.pApplicationName = "Fishy Engine",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "Fishy",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = vk::ApiVersion14 // Construct using 1.3 for broad compatibility
	};

	// Get the required layers
	std::vector<char const *> requiredLayers;
	if (enableValidationLayers) {
		requiredLayers.assign(validationLayers.begin(), validationLayers.end());
	}

	// Check if the required layers are supported by the Vulkan implementation.
	// Note: enumerateInstanceLayerProperties() returns
	// std::vector<vk::LayerProperties> but the user logic iterates and checks.
	auto layerProperties = _context.enumerateInstanceLayerProperties();
	for (auto const &requiredLayer : requiredLayers) {
		if (std::ranges::none_of(layerProperties, [requiredLayer](auto const &layerProperty) {
				return strcmp(layerProperty.layerName, requiredLayer) == 0;
			})) {
			throw std::runtime_error("Required layer not supported: " + std::string(requiredLayer));
		}
	}

	// Get the required extensions.
	auto requiredExtensions = getRequiredExtensions();

	// Check if the required extensions are supported by the Vulkan
	// implementation.
	auto extensionProperties = _context.enumerateInstanceExtensionProperties();
	for (auto const &requiredExtension : requiredExtensions) {
		if (std::ranges::none_of(extensionProperties, [requiredExtension](auto const &extensionProperty) {
				return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
			})) {
			throw std::runtime_error("Required extension not supported: " + std::string(requiredExtension));
		}
	}

	vk::InstanceCreateInfo createInfo{.pApplicationInfo = &appInfo,
									  .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
									  .ppEnabledLayerNames = requiredLayers.data(),
									  .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
									  .ppEnabledExtensionNames = requiredExtensions.data()};

	// Use member _instance and _entry
	_instance = vk::raii::Instance(_context, createInfo);
}

void VulkanContext::setupDebugMessenger() {
	// TODO: Implement Debug Messenger
	if (!enableValidationLayers)
		return;

	vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
														vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
														vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
	vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
													   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
													   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
	vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
		.messageSeverity = severityFlags, .messageType = messageTypeFlags, .pfnUserCallback = &debugCallback};
	_debugMessenger = _instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

void VulkanContext::createSurface(GLFWwindow *window) {
	VkSurfaceKHR cSurface;
	if (glfwCreateWindowSurface(*_instance, window, nullptr, &cSurface) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create window surface!");
	}
	// Wrap the C surface handle into RAII wrapper
	_surface = vk::raii::SurfaceKHR(_instance, cSurface);
}

void VulkanContext::pickPhysicalDevice() {
	std::vector<vk::raii::PhysicalDevice> devices = _instance.enumeratePhysicalDevices();
	const auto devIter = std::ranges::find_if(devices, [&](auto const &device) {
		// Check if the device supports the Vulkan 1.3 API version
		bool supportsVulkan1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

		// Check if any of the queue families support graphics operations
		auto queueFamilies = device.getQueueFamilyProperties();
		bool supportsGraphics = std::ranges::any_of(
			queueFamilies, [](auto const &qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

		// Check if all required device extensions are available
		auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
		bool supportsAllRequiredExtensions = std::ranges::all_of(
			requiredDeviceExtension, [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
				return std::ranges::any_of(
					availableDeviceExtensions, [requiredDeviceExtension](auto const &availableDeviceExtension) {
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
		_chosenGPU = *devIter;
	} else {
		throw std::runtime_error("failed to find a suitable GPU!");
	}
}

void VulkanContext::createLogicalDevice() {
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = _chosenGPU.getQueueFamilyProperties();

	// get the first index into queueFamilyProperties which supports both
	// graphics and present
	for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++) {
		if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
			_chosenGPU.getSurfaceSupportKHR(qfpIndex, *_surface)) {
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

	_device = vk::raii::Device(_chosenGPU, deviceCreateInfo);
	_graphicsQueue = vk::raii::Queue(_device, _graphicsQueueFamily, 0);
}

} // namespace Fishy