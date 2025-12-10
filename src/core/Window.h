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

	Window(const Properties &properties, vk::raii::Instance &instance);
	~Window();

	void Update();
	bool ShouldClose() const;
	GLFWwindow *nativeWindow() const { return _window; }

	uint32_t width() const { return _properties.width; }
	uint32_t height() const { return _properties.height; }

	vk::raii::SurfaceKHR &surface() { return _surface; }

private:
	void Init(const Properties &properties);
	void createSurface(vk::raii::Instance &instance);
	static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
	static void FramebufferResizeCallback(GLFWwindow *window, int width, int height);

	vk::raii::SurfaceKHR _surface = nullptr;

	GLFWwindow *_window = nullptr;
	Properties _properties;

	// Fullscreen state
	bool _isFullscreen = false;
	int _windowedXPos = 0;
	int _windowedYPos = 0;
	int _windowedWidth = 0;
	int _windowedHeight = 0;
};

} // namespace Fishy
