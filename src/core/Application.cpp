#include "Application.h"

#include "../ui/DebugPanel.h"
#include "LogSystem.h"
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

	LogSystem::get().info("Initializing Application...");
	LogSystem::get().info("Window created: {}x{}", _window.getWidth(), _window.getHeight());

	// Initialize ImGui Layer
	_imguiLayer = std::make_unique<ImGuiLayer>(_context, _device, _window, _renderer.getSwapChainFormat());
	LogSystem::get().info("ImGui Layer initialized");

	// Try to load a default model
	auto modelPath = "assets/models/DamagedHelmet.glb";
	LogSystem::get().info("Loading default model: {}", modelPath);
	_model = ModelLoader::loadModel(modelPath, &_resourceManager);

	if (!_model) {
		LogSystem::get().warn("Failed to load model, creating default geometry");
		_model = createDefaultModel();
	}

	// Load IBL environment
	try {
		_iblEnvironment = IBLEnvironment::load(_device, "assets/environments/field");
		_renderer.setIBLEnvironment(_iblEnvironment.get());
	} catch (const std::exception& e) {
		LogSystem::get().warn("Failed to load IBL environment: {}", e.what());
	}

	LogSystem::get().info("Application initialization complete");
}

Application::~Application() {
	LogSystem::get().info("Shutting down Application...");
	// Wait for GPU before destroying ImGui resources
	_device->waitIdle();
	_imguiLayer.reset();
	LogSystem::get().info("Application shutdown complete");
}

// Main application loop
void Application::run() {
	LogSystem::get().info("Starting main render loop");
	DebugPanel debugPanel;

	// Frame timing
	auto lastFrameTime = std::chrono::high_resolution_clock::now();

	while (!_window.shouldClose()) {
		// Calculate delta time
		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
		lastFrameTime = currentTime;

		_window.update();

		// Update camera from mouse input (only if ImGui is not capturing mouse)
		ImGuiIO& io = ImGui::GetIO();
		if (!io.WantCaptureMouse) {
			const auto& mouseState = _window.getMouseState();
			_renderer.getCamera().update(static_cast<float>(mouseState.deltaX), static_cast<float>(mouseState.deltaY),
										 static_cast<float>(mouseState.scrollDelta), mouseState.rightButton,
										 mouseState.middleButton, mouseState.leftButton, deltaTime);
		}

		// Reset mouse delta for next frame
		_window.resetMouseDelta();

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

	LogSystem::get().info("Main render loop ended");
	_device->waitIdle();
}

std::shared_ptr<Model> Application::createDefaultModel() { // Create a simple default model with two quads
	auto model = std::make_shared<Model>();

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

	model->addPrimitive({mesh, material});

	return model;
}

} // namespace Fishy
