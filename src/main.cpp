#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>

#include "core/VulkanContext.h"
#include "core/Window.h"
#include <iostream>
#include <vulkan/vulkan.hpp>

int main() {
	try {
		Fishy::Window::Properties props;
		props.title = "Fishy Engine";
		// props.width = 800;
		// props.height = 600;

		Fishy::Window window(props);

		Fishy::VulkanContext context;
		context.Init(window.GetNativeWindow());

		// Optional: Check Vulkan extensions just to keep previous behavior verification
		// We need to use vulkan.hpp C++ API if we included <vulkan/vulkan.hpp>
		// or use C API if we use vk::... with cast.
		// using vulkan.hpp:
		std::vector<vk::ExtensionProperties> extensions = vk::enumerateInstanceExtensionProperties();
		std::cout << extensions.size() << " extensions supported" << std::endl;

		std::cout << "Hello Fishy! Build successful." << std::endl;

		while (!window.ShouldClose()) {
			window.Update();
		}
	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}
