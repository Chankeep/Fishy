#include "Window.h"
#include <iostream>
#include <stdexcept>

namespace Fishy {

Window::Window(const Properties &properties) : _properties(properties) { Init(properties); }

Window::~Window() {
	if (_window) {
		glfwDestroyWindow(_window);
	}
	glfwTerminate();
}

void Window::Init(const Properties &properties) {
	if (!glfwInit()) {
		throw std::runtime_error("Failed to initialize GLFW");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, properties.resizable ? GLFW_TRUE : GLFW_FALSE);

	_window = glfwCreateWindow(properties.width, properties.height, properties.title.c_str(), nullptr, nullptr);
	if (!_window) {
		glfwTerminate();
		throw std::runtime_error("Failed to create GLFW window");
	}

	glfwSetWindowUserPointer(_window, this);
	glfwSetKeyCallback(_window, KeyCallback);
	glfwSetFramebufferSizeCallback(_window, FramebufferResizeCallback);
}

void Window::Update() { glfwPollEvents(); }

bool Window::ShouldClose() const { return glfwWindowShouldClose(_window); }

void Window::FramebufferResizeCallback(GLFWwindow *window, int width, int height) {
	auto app = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
	app->_properties.width = width;
	app->_properties.height = height;
}

void Window::KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
	Window *win = static_cast<Window *>(glfwGetWindowUserPointer(window));

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
		// Toggle fullscreen
		if (win->_isFullscreen) {
			glfwSetWindowMonitor(window, nullptr, win->_windowedXPos, win->_windowedYPos, win->_windowedWidth,
								 win->_windowedHeight, 0);
			win->_isFullscreen = false;
		} else {
			glfwGetWindowPos(window, &win->_windowedXPos, &win->_windowedYPos);
			glfwGetWindowSize(window, &win->_windowedWidth, &win->_windowedHeight);

			GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
			const GLFWvidmode *mode = glfwGetVideoMode(primaryMonitor);

			glfwSetWindowMonitor(window, primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
			win->_isFullscreen = true;
		}
	}
}

} // namespace Fishy
