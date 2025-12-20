#define VOLK_IMPLEMENTATION
#include "VulkanContext.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Fishy {

// Define validation layers
const std::vector<char const*> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VulkanContext::VulkanContext() { Init(); }

VulkanContext::~VulkanContext() { Cleanup(); }

void VulkanContext::Init() {
	// Initialize volk
	if (volkInitialize() != VK_SUCCESS) {
		throw std::runtime_error("Failed to initialize volk!");
	}

	VULKAN_HPP_DEFAULT_DISPATCHER.init(_context.getDispatcher()->vkGetInstanceProcAddr);

	createInstance();
	setupDebugMessenger();
}

void VulkanContext::Cleanup() {
	// RAII handles handle destruction automatically.
	// Destroyed in reverse order of declaration in header.
}

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
													  vk::DebugUtilsMessageTypeFlagsEXT type,
													  const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
													  void*) {
	if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
		severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
		std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
	}

	return vk::False;
}

// Create the Vulkan Instance using vk-bootstrap
void VulkanContext::createInstance() {
	vkb::InstanceBuilder builder;
	builder.set_app_name("Fishy Engine").set_engine_name("Fishy").require_api_version(1, 3, 0); // Require Vulkan 1.3

	if (enableValidationLayers) {
		builder.request_validation_layers();
	}

	auto system_info_ret = vkb::SystemInfo::get_system_info();
	if (!system_info_ret) {
		throw std::runtime_error(system_info_ret.error().message());
	}
	auto system_info = system_info_ret.value();

	if (system_info.validation_layers_available) {
		builder.enable_validation_layers();
	}

	auto vkb_inst_ret = builder.build();
	if (!vkb_inst_ret) {
		throw std::runtime_error(vkb_inst_ret.error().message());
	}
	vkb::Instance vkb_inst = vkb_inst_ret.value();

	volkLoadInstance(vkb_inst.instance);

	// Load Vulkan functions via volk into a context
	_context = vk::raii::Context();

	// Create RAII Instance from existing handle
	_instance = vk::raii::Instance(_context, vkb_inst.instance);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(*_instance);
}

void VulkanContext::setupDebugMessenger() {
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