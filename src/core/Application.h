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

	void Run();

private:
	// Order matters for RAII destruction!
	// Destruction order is reverse of declaration order.

	struct GlfwInitializer {
		GlfwInitializer();
		~GlfwInitializer();
	} _glfwInitializer;

	// 1. Context (Instance) - Last to be destroyed
	Fishy::VulkanContext _context;

	// 2. Window (Surface) - Destroyed before Instance
	Fishy::Window _window;

	// 3. Device - Destroyed before Surface
	Fishy::VulkanDevice _device;

	// 4. Resource Manager - Destroyed before Device
	Fishy::ResourceManager _resourceManager;

	// 5. Renderer - Owns SwapChain, destroyed before Device
	Fishy::Renderer _renderer;

	// 6. ImGui Layer - Destroyed before Renderer
	std::unique_ptr<ImGuiLayer> _imguiLayer;

	// 7. Loaded model
	std::shared_ptr<Model> _model;

	// 8. IBL Environment
	std::unique_ptr<IBLEnvironment> _iblEnvironment;
};

} // namespace Fishy
