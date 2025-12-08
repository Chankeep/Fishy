#pragma once
#include <optional>
#include <vector>
#define VULKAN_HPP_NO_CONSTRUCTORS // 移除Vulkan.hpp的构造函数

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

struct GLFWwindow;

namespace Fishy {
class VulkanContext {
public:
	void Init(GLFWwindow *window);
	void Cleanup();

	vk::Instance GetInstance() const { return *_instance; }
	vk::Device GetDevice() const { return *_device; }
	vk::PhysicalDevice GetPhysicalDevice() const { return *_chosenGPU; }
	vk::Queue GetGraphicsQueue() const { return *_graphicsQueue; }
	uint32_t GetGraphicsQueueFamily() const { return _graphicsQueueFamily; }
	vk::SurfaceKHR GetSurface() const { return *_surface; }

private:
	vk::raii::Context _context;
	vk::raii::Instance _instance = nullptr;
	vk::raii::DebugUtilsMessengerEXT _debugMessenger = nullptr;
	vk::raii::SurfaceKHR _surface = nullptr;

	vk::raii::PhysicalDevice _chosenGPU = nullptr;
	vk::raii::Device _device = nullptr;

	vk::raii::Queue _graphicsQueue = nullptr;
	uint32_t _graphicsQueueFamily;

	// Helper functions
	void createInstance();
	void setupDebugMessenger();
	void createSurface(GLFWwindow *window);
	void pickPhysicalDevice();
	void createLogicalDevice();
};
} // namespace Fishy