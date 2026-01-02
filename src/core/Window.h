#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <GLFW/glfw3.h>
#include <string>

namespace Fishy {

/**
 * @brief Mouse input state for camera controls
 */
struct MouseState {
	double deltaX = 0.0;	   // Mouse X movement since last frame
	double deltaY = 0.0;	   // Mouse Y movement since last frame
	double scrollDelta = 0.0;  // Mouse wheel scroll delta
	bool rightButton = false;  // Right mouse button held
	bool middleButton = false; // Middle mouse button held
	bool leftButton = false;   // Left mouse button held

	// Reset per-frame deltas
	void reset() {
		deltaX = 0.0;
		deltaY = 0.0;
		scrollDelta = 0.0;
	}
};

/**
 * @brief Manages the GLFW window and Vulkan surface.
 *
 * Handles window creation, event callbacks (resizing, input), and creates
 * the corresponding Vulkan surface. Also tracks mouse input for camera controls.
 */
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
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	void update();
	[[nodiscard]] bool shouldClose() const;
	[[nodiscard]] GLFWwindow* getNativeWindow() const { return _window; }

	[[nodiscard]] uint32_t getWidth() const { return _properties.width; }
	[[nodiscard]] uint32_t getHeight() const { return _properties.height; }
	[[nodiscard]] vk::Extent2D getExtent() const { return {_properties.width, _properties.height}; }

	[[nodiscard]] bool wasWindowResized() const { return _framebufferResized; }

	[[nodiscard]] vk::raii::SurfaceKHR& getSurface() { return _surface; }
	[[nodiscard]] const vk::raii::SurfaceKHR& getSurface() const { return _surface; }

	[[nodiscard]] const MouseState& getMouseState() const { return _mouseState; }
	void resetWindowResizedFlag() { _framebufferResized = false; }
	void resetMouseDelta() { _mouseState.reset(); }

private:
	void init(const Properties& properties);
	void createSurface(const vk::raii::Instance& instance);
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
	static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

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

	// Mouse tracking state
	MouseState _mouseState;
	double _lastMouseX = 0.0;
	double _lastMouseY = 0.0;
	bool _firstMouse = true;
};

} // namespace Fishy
