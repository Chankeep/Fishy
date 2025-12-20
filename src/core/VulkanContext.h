#pragma once
#include <optional>
#include <vector>

#define VULKAN_HPP_NO_CONSTRUCTORS // Disable constructors for Vulkan.hpp

#include <volk.h>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

struct GLFWwindow;

namespace Fishy {
/**
 * @brief Manages the Vulkan Instance and Debug Messenger.
 *
 * Initializes the Vulkan loader (volk), creates the Vulkan instance,
 * and sets up validation layers if enabled.
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
	void setupDebugMessenger();

	vk::raii::Context _context;
	vk::raii::Instance _instance = nullptr;
	vk::raii::DebugUtilsMessengerEXT _debugMessenger = nullptr;
};
} // namespace Fishy