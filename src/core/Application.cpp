#include "Application.h"
#include <iostream>

#include "../ui/DebugPanel.h"
#include <imgui.h>

namespace Fishy {

Application::GlfwInitializer::GlfwInitializer() {
	if (!glfwInit()) {
		throw std::runtime_error("Failed to initialize GLFW");
	}
}

Application::GlfwInitializer::~GlfwInitializer() { glfwTerminate(); }

Application::Application()
	: _glfwInitializer(), _context(),
	  _window({.title = "Fishy Engine", .width = 1280, .height = 720}, _context.getInstance()),
	  _device(_context.getInstance(), _window.getSurface()), _resourceManager(_device),
	  _renderer(_device, _window, _resourceManager) {

	// Initialize ImGui Layer
	_imguiLayer = std::make_unique<ImGuiLayer>(_context, _device, _window, _renderer.getSwapChainFormat());

	// Try to load a default model
	_model = ModelLoader::loadModel("assets/models/DamagedHelmet.glb", &_resourceManager);

	if (!_model) {
		std::cout << "No model loaded. Creating default geometry..." << std::endl;
		// Create a simple default model with two quads
		_model = std::make_shared<Model>();

		std::vector<Vertex> vertices = {
			// Front quad
			{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},

			// Back quad
			{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
			{{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
		};

		std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

		auto mesh = std::make_shared<Mesh>(vertices, indices);
		auto material = std::make_shared<Material>();

		_model->addPrimitive({mesh, material});
	}
}

Application::~Application() {
	// Wait for GPU before destroying ImGui resources
	_device->waitIdle();
	_imguiLayer.reset();
}

// Main application loop
void Application::Run() {
	DebugPanel debugPanel;

	while (!_window.ShouldClose()) {
		_window.Update();

		// Start ImGui frame
		_imguiLayer->newFrame();

		// Build UI
		debugPanel.draw();

		// Update debug settings in renderer
		const auto& settings = debugPanel.getSettings();
		_renderer.setDebugSettings(static_cast<float>(settings.debugViewInputs),
								   static_cast<float>(settings.debugViewEquation));

		// Render scene with UI callback
		_renderer.render(*_model, [this](VkCommandBuffer cmd) { _imguiLayer->render(cmd); });
	}

	_device->waitIdle();
}

} // namespace Fishy
