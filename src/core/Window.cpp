#include "Window.h"
#include <iostream>
#include <stdexcept>

namespace Fishy {

Window::Window(const Properties& properties, const vk::raii::Instance& instance) : _properties(properties) {
	Init(properties);
	createSurface(instance);
}

Window::~Window() {
	if (_window) {
		glfwDestroyWindow(_window);
		_window = nullptr;
	}
	// Note: glfwTerminate() should be called in main()
}

void Window::Init(const Properties& properties) {
	// Note: glfwInit() should be called before creating VulkanContext
	// This allows GLFW to properly provide the required Vulkan extensions

	// Force X11 platform on Linux/WSL to ensure window decorations work properly
	// Wayland in WSLg sometimes doesn't display window decorations correctly
#ifdef __linux__
	glfwWindowHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

	// Stop GLFW from creating an OpenGL context
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, properties.resizable ? GLFW_TRUE : GLFW_FALSE);

	_window = glfwCreateWindow(properties.width, properties.height, properties.title.c_str(), nullptr, nullptr);
	if (!_window) {
		LogSystem::get().error("Failed to create GLFW window: {} ({}x{})",
			properties.title, properties.width, properties.height);
		throw std::runtime_error("Failed to create GLFW window");
	}

	LogSystem::get().info("Created GLFW window: {} ({}x{})", properties.title, properties.width, properties.height);

	glfwSetWindowUserPointer(_window, this);
	glfwSetKeyCallback(_window, KeyCallback);
	glfwSetFramebufferSizeCallback(_window, FramebufferResizeCallback);
	glfwSetMouseButtonCallback(_window, MouseButtonCallback);
	glfwSetCursorPosCallback(_window, CursorPosCallback);
	glfwSetScrollCallback(_window, ScrollCallback);
}

void Window::createSurface(const vk::raii::Instance& instance) {
	VkSurfaceKHR cSurface;
	if ((glfwCreateWindowSurface(*instance, _window, nullptr, &cSurface) != 0)) {
		LogSystem::get().error("Failed to create window surface");
		throw std::runtime_error("Failed to create window surface!");
	}
	// Wrap the C surface handle into RAII wrapper
	_surface = vk::raii::SurfaceKHR(instance, cSurface);
	LogSystem::get().info("Vulkan surface created successfully");
}

void Window::Update() { glfwPollEvents(); }

bool Window::ShouldClose() const { return glfwWindowShouldClose(_window); }

void Window::FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	app->_properties.width = width;
	app->_properties.width = width;
	app->_properties.height = height;
	app->_framebufferResized = true;
	LogSystem::get().info("Framebuffer resized: {}x{}", width, height);
}

void Window::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
		// Toggle fullscreen
		if (win->_isFullscreen) {
			glfwSetWindowMonitor(window, nullptr, win->_windowedXPos, win->_windowedYPos, win->_windowedWidth,
								 win->_windowedHeight, 0);
			win->_isFullscreen = false;
			LogSystem::get().info("Exited fullscreen mode");
		} else {
			glfwGetWindowPos(window, &win->_windowedXPos, &win->_windowedYPos);
			glfwGetWindowSize(window, &win->_windowedWidth, &win->_windowedHeight);

			GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

			glfwSetWindowMonitor(window, primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
			win->_isFullscreen = true;
			LogSystem::get().info("Entered fullscreen mode: {}x{} @ {}Hz",
				mode->width, mode->height, mode->refreshRate);
		}
	}
}

void Window::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));

	if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		win->_mouseState.rightButton = (action == GLFW_PRESS || action == GLFW_REPEAT);
	}
	if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
		win->_mouseState.middleButton = (action == GLFW_PRESS || action == GLFW_REPEAT);
	}
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		win->_mouseState.leftButton = (action == GLFW_PRESS || action == GLFW_REPEAT);
	}

	// Reset first mouse state when any button is pressed to avoid jump
	if (action == GLFW_PRESS) {
		win->_firstMouse = true;
	}
}

void Window::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
	Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));

	if (win->_firstMouse) {
		win->_lastMouseX = xpos;
		win->_lastMouseY = ypos;
		win->_firstMouse = false;
	}

	win->_mouseState.deltaX = xpos - win->_lastMouseX;
	win->_mouseState.deltaY = ypos - win->_lastMouseY;

	win->_lastMouseX = xpos;
	win->_lastMouseY = ypos;
}

void Window::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
	// yoffset is the scroll delta (positive = scroll up/zoom in, negative = scroll down/zoom out)
	win->_mouseState.scrollDelta = yoffset;
}

} // namespace Fishy
