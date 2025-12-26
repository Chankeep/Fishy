#define VOLK_IMPLEMENTATION
#include "VulkanContext.h"
#include "LogSystem.h"

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
	LogSystem::get().info("Initializing Vulkan context...");

	// Initialize volk
	if (volkInitialize() != VK_SUCCESS) {
		LogSystem::get().error("Failed to initialize volk!");
		throw std::runtime_error("Failed to initialize volk!");
	}

	VULKAN_HPP_DEFAULT_DISPATCHER.init(_context.getDispatcher()->vkGetInstanceProcAddr);

	LogSystem::get().info("volk initialized successfully");
	createInstance();
}

void VulkanContext::Cleanup() {
	LogSystem::get().info("Cleaning up Vulkan context...");

	// Must explicitly destroy vk-bootstrap's debug messenger before the RAII instance destructs
	// Otherwise we get validation errors about undestroyed objects
	if (_vkbInstance.debug_messenger != VK_NULL_HANDLE) {
		vkb::destroy_debug_utils_messenger(_vkbInstance.instance, _vkbInstance.debug_messenger);
		LogSystem::get().info("Destroyed Vulkan debug messenger");
	}

	// RAII handles destruction automatically in reverse order of declaration
}

// Create the Vulkan Instance using vk-bootstrap
void VulkanContext::createInstance() {
	LogSystem::get().info("Creating Vulkan instance...");

	vkb::InstanceBuilder builder;
	builder.set_app_name("Fishy Engine").set_engine_name("Fishy").require_api_version(1, 3, 0); // Require Vulkan 1.3

	if (enableValidationLayers) {
		LogSystem::get().info("Validation layers enabled");
		builder.request_validation_layers().use_default_debug_messenger().set_debug_callback(LogSystem::vulkan_debug_callback).set_debug_callback_user_data_pointer(LogSystem::get().logger().get()); // Let vk-bootstrap handle debug messenger
	}

	auto system_info_ret = vkb::SystemInfo::get_system_info();
	if (!system_info_ret) {
		LogSystem::get().error("Failed to get system info: {}", system_info_ret.error().message());
		throw std::runtime_error(system_info_ret.error().message());
	}
	auto system_info = system_info_ret.value();

	if (system_info.validation_layers_available && enableValidationLayers) {
		builder.enable_validation_layers();
	}

	auto vkb_inst_ret = builder.build();
	if (!vkb_inst_ret) {
		LogSystem::get().error("Failed to build Vulkan instance: {}", vkb_inst_ret.error().message());
		throw std::runtime_error(vkb_inst_ret.error().message());
	}
	_vkbInstance = vkb_inst_ret.value();

	volkLoadInstance(_vkbInstance.instance);
	LogSystem::get().info("Vulkan instance created successfully");

	// Create RAII Instance from existing handle
	// Note: _context is already initialized in the class, no need to recreate
	_instance = vk::raii::Instance(_context, _vkbInstance.instance);

	// Reinitialize the dispatcher with volk's loaded instance functions
	VULKAN_HPP_DEFAULT_DISPATCHER.init(*_instance, vkGetInstanceProcAddr);
}

} // namespace Fishy