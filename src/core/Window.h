#pragma once

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

	Window(const Properties &properties);
	~Window();

	void Update();
	bool ShouldClose() const;
	GLFWwindow *GetNativeWindow() const { return _window; }

	uint32_t GetWidth() const { return _properties.width; }
	uint32_t GetHeight() const { return _properties.height; }

private:
	void Init(const Properties &properties);
	static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
	static void FramebufferResizeCallback(GLFWwindow *window, int width, int height);

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
