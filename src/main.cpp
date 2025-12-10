#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>

#include "core/SwapChain.h"
#include "core/VulkanContext.h"
#include "core/VulkanDevice.h"
#include "core/Window.h"
#include <iostream>
#include <vulkan/vulkan.hpp>

int main() {

	try {
		// Initialize GLFW FIRST (before creating Vulkan instance)
		if (!glfwInit()) {
			throw std::runtime_error("Failed to initialize GLFW");
		}

		Fishy::Window::Properties props;
		props.title = "Fishy Engine";
		props.width = 1280;
		props.height = 720;

		// Now create Vulkan context (which will query GLFW for required extensions)
		Fishy::VulkanContext context;
		
		// Then create the window with the instance
		Fishy::Window window(props, context.instance());
		Fishy::VulkanDevice device(context.instance(), window.surface());

		int width, height;
		glfwGetFramebufferSize(window.nativeWindow(), &width, &height);

		// ✅ 保存 SwapChain 对象（不能是临时对象）
		auto swapChain = std::make_unique<Fishy::SwapChain>(device.device(), device.physicalDevice(), window.surface(),
															width, height);

		while (!window.ShouldClose()) {
			window.Update();
			// 在这里执行渲染命令
		}

		// 显式销毁（或让 unique_ptr 自动销毁）
		swapChain.reset();
		
		// Cleanup GLFW
		glfwTerminate();

	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return -1;
	}
	return 0;
}
