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

	vk::raii::Context _context;
	vk::raii::Instance _instance = nullptr;
	vkb::Instance _vkbInstance; // Stores vk-bootstrap instance for debug messenger cleanup
};
} // namespace Fishy