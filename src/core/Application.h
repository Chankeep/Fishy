#pragma once

#include "../ecs/systems/CameraSystem.h"
#include "../ecs/systems/CullingSystem.h"
#include "../ecs/systems/LightingSystem.h"
#include "../ecs/systems/RenderSystem.h"
#include "../ecs/systems/TransformSystem.h"
#include "../ecs/systems/OrbitControllerSystem.h"
#include "../rendering/Renderer.h"
#include "../resources/IBLEnvironment.h"
#include "../resources/ModelLoader.h"
#include "../resources/ResourceManager.h"
#include "../scene/Scene.h"
#include "../ui/ImGuiLayer.h"
#include "../ui/EditorLayer.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "Window.h"


#include <memory>

namespace Fishy {

/**
 * @brief Main application class.
 *
 * Coordinates the engine lifecycle: initialization, main loop, and cleanup.
 * Manages core subsystems like Window, VulkanContext, Device, Renderer, and ECS Systems.
 */
class Application {
public:
	Application();
	~Application();
	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	void run();

private:
	void createDefaultScene();

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
	EditorLayer _editorLayer;
	std::unique_ptr<IBLEnvironment> _iblEnvironment;

	// ECS
	std::unique_ptr<Scene> _scene;
	TransformSystem _transformSystem;
	OrbitControllerSystem _orbitControllerSystem;
	CameraSystem _cameraSystem;
	CullingSystem _cullingSystem;
	LightingSystem _lightingSystem;
	RenderSystem _renderSystem;

};

} // namespace Fishy
