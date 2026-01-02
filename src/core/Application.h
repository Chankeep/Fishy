#pragma once

#include "../renderer/Renderer.h"
#include "../resources/IBLEnvironment.h"
#include "../resources/Model.h"
#include "../resources/ModelLoader.h"
#include "../resources/ResourceManager.h"
#include "../ui/ImGuiLayer.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "Window.h"

#include <memory>

namespace Fishy {

/**
 * @brief Main application class.
 *
 * Coordinates the engine lifecycle: initialization, main loop, and cleanup.
 * Manages core subsystems like Window, VulkanContext, Device, and Renderer.
 */
class Application {
public:
	Application();
	~Application();
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	void run();

private:
	std::shared_ptr<Model> createDefaultModel();

	// Order matters for RAII destruction!
	// Destruction order is reverse of declaration order.
	struct GlfwInitializer {
		GlfwInitializer();
		~GlfwInitializer();
	} _glfwInitializer;

	Fishy::VulkanContext _context;
	Fishy::Window _window;
	Fishy::VulkanDevice _device;
	Fishy::ResourceManager _resourceManager;
	Fishy::Renderer _renderer;
	std::unique_ptr<ImGuiLayer> _imguiLayer;
	std::shared_ptr<Model> _model;
	std::unique_ptr<IBLEnvironment> _iblEnvironment;
};

} // namespace Fishy
