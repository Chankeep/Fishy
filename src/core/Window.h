#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS // 移除Vulkan.hpp的构造函数

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <GLFW/glfw3.h>
#include <string>

namespace Fishy {

class Window {
public:
	struct Properties {
		std::string title = "Fishy Engine";
		uint32_t width = 1280;
		uint32_t height = 720;
		bool resizable = true;
	};

	Window(const Properties& properties, const vk::raii::Instance& instance);
	~Window();

	void Update();
	bool ShouldClose() const;
	GLFWwindow* getNativeWindow() const { return _window; }

	uint32_t getWidth() const { return _properties.width; }
	uint32_t getHeight() const { return _properties.height; }
	vk::Extent2D getExtent() const { return {_properties.width, _properties.height}; }

	bool wasWindowResized() const { return _framebufferResized; }
	void resetWindowResizedFlag() { _framebufferResized = false; }

	vk::raii::SurfaceKHR& getSurface() { return _surface; }

private:
	void Init(const Properties& properties);
	void createSurface(const vk::raii::Instance& instance);
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

	vk::raii::SurfaceKHR _surface = nullptr;

	GLFWwindow* _window = nullptr;
	Properties _properties;

	// Fullscreen state
	bool _isFullscreen = false;
	int _windowedXPos = 0;
	int _windowedYPos = 0;
	int _windowedWidth = 0;
	int _windowedHeight = 0;

	bool _framebufferResized = false;
};

} // namespace Fishy
