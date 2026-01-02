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
 * and sets up validation layers with custom logging integration.
 */
class VulkanContext {
public:
	VulkanContext();
	~VulkanContext();
	VulkanContext(const VulkanContext&) = delete;
	VulkanContext& operator=(const VulkanContext&) = delete;

	[[nodiscard]] vk::raii::Instance& getInstance() { return _instance; }
	[[nodiscard]] const vk::raii::Instance& getInstance() const { return _instance; }

private:
	void init();
	void cleanup();

	// Helper functions
	void createInstance();

	vk::raii::Context _context;
	vk::raii::Instance _instance = nullptr;
	vkb::Instance _vkbInstance; // Stores vk-bootstrap instance for debug messenger cleanup
};

} // namespace Fishy