#pragma once

#include <volk.h>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <imgui.h>

struct GLFWwindow;

namespace Fishy {

class VulkanContext;
class VulkanDevice;
class Window;

/**
 * @brief Manages Dear ImGui integration with Vulkan.
 *
 * Handles ImGui initialization, per-frame updates, and rendering
 * using Vulkan Dynamic Rendering.
 */
class ImGuiLayer {
public:
	ImGuiLayer(VulkanContext& context, VulkanDevice& device, Window& window, vk::Format renderPassFormat);
	~ImGuiLayer();

	// Non-copyable
	ImGuiLayer(const ImGuiLayer&) = delete;
	ImGuiLayer& operator=(const ImGuiLayer&) = delete;

	/**
	 * @brief Start a new ImGui frame.
	 * Call this at the beginning of your frame before any ImGui calls.
	 */
	void newFrame();

	/**
	 * @brief Render ImGui draw data.
	 * Call this after ImGui::Render() to record draw commands.
	 */
	void render(VkCommandBuffer cmd);

private:
	void initVulkanResources();
	void initImGui();

	VulkanContext& _context;
	VulkanDevice& _device;
	Window& _window;

	VkFormat _renderPassFormat;
	VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
};

} // namespace Fishy
