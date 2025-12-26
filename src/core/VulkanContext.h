#pragma once
#include <optional>
#include <vector>

#include <VkBootstrap.h>
#include <volk.h>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

struct GLFWwindow;

namespace Fishy {

/**
 * @brief Vulkan debug callback that uses our Log system
 *
 * Maps Vulkan validation messages to appropriate log levels.
 * @param messageSeverity Severity of the message (VK_DEBUG_UTILS_MESSAGE_SEVERITY_*)
 * @param messageType Type of the message (VK_DEBUG_UTILS_MESSAGE_TYPE_*)
 * @param pCallbackData Debug message data
 * @param pUserData User data (unused)
 * @return VK_FALSE to not bail out of the validation layer
 */
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

/**
 * @brief Manages the Vulkan Instance and Debug Messenger.
 *
 * Initializes the Vulkan loader (volk), creates the Vulkan instance,
 * and sets up validation layers with custom logging integration.
 */
class VulkanContext {
public:
	VulkanContext();
	~VulkanContext();

	vk::raii::Instance& getInstance() { return _instance; }

private:
	void Init();
	void Cleanup();

	// Helper functions
	void createInstance();

	vk::raii::Context _context;
	vk::raii::Instance _instance = nullptr;
	vkb::Instance _vkbInstance;  // Stores vk-bootstrap instance for debug messenger cleanup
	VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;  // Custom debug messenger
};

} // namespace Fishy