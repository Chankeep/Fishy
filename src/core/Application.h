#pragma once

#include "../renderer/Renderer.h"
#include "ResourceManager.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "Window.h"

#include <memory>

namespace Fishy {

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

	// 4. Resource Manager (Shader Modules) - Destroyed before Device
	Fishy::ResourceManager _resourceManager;

	// 5. Renderer - Owns SwapChain, destroyed before Device
	Fishy::Renderer _renderer;
};

} // namespace Fishy
