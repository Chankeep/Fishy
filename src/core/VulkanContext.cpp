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



#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VulkanContext::VulkanContext(){
	Init();
}

VulkanContext::~VulkanContext(){
	Cleanup();
}

void VulkanContext::Init() {
	// Initialize RAII Context (loads Vulkan library)
	_context = vk::raii::Context();

	createInstance();
	setupDebugMessenger();
}

void VulkanContext::Cleanup() {
	// RAII handles handle destruction automatically.
	// Destroyed in reverse order of declaration in header.
}

std::vector<const char *> getRequiredExtensions() {
	uint32_t glfwExtensionCount = 0;
	const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	
	if (!glfwExtensions) {
		std::cerr << "WARNING: glfwGetRequiredInstanceExtensions returned nullptr" << std::endl;
		// Fallback - manually add common surface extensions
		std::vector<const char *> extensions;
		extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef VK_USE_PLATFORM_XLIB_KHR
		extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
		extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		return extensions;
	}

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



} // namespace Fishy