#include "Application.h"

#include "../ecs/Entity.h"
#include "../ecs/components/CameraComponent.h"
#include "../ecs/components/LightComponent.h"
#include "../ecs/components/MeshComponent.h"
#include "../ecs/components/OrbitControllerComponent.h"
#include "../ecs/components/MeshRendererComponent.h"
#include "../ecs/components/TransformComponent.h"
#include "../ecs/components/HierarchyComponent.h"
#include "../ecs/utils/TransformUtils.h"
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

	FISHY_LOG_TRACE("Initializing Application...");
	FISHY_LOG_TRACE("Window created: {}x{}", _window.getWidth(), _window.getHeight());

	// Initialize ImGui Layer
	_imguiLayer = std::make_unique<ImGuiLayer>(_context, _device, _window, _renderer.getSwapChainFormat());
	FISHY_LOG_TRACE("ImGui Layer initialized");

	// Initialize Scene
	_scene = std::make_unique<Scene>();
	FISHY_LOG_TRACE("ECS Scene initialized");

	// Try to load a default model into the scene
	auto modelPath = "assets/models/DamagedHelmet.glb";
	auto planePath = "assets/models/ABeautifulGame.glb";
	FISHY_LOG_INFO("Loading default model: {}", modelPath);
	auto result = ModelLoader::loadModelIntoScene(planePath, *_scene, _resourceManager);
	if (!result) {
		FISHY_LOG_WARN("Failed to load model: {}. Using fallback geometry.", result.error().message());
		createDefaultScene();
	} else {
		// Create a single root entity for the entire model
		Entity modelRoot = _scene->createEntity("ChessBoardRoot");
		auto& rootTransform = modelRoot.getComponent<TransformComponent>();
		rootTransform.setScale({10.0f, 10.0f, 10.0f});

		// Reparent all loaded ROOT entities to the model root
		for (auto& entity : result.value()) {
			bool isRoot = true;
			if (entity.hasComponent<HierarchyComponent>()) {
				isRoot = (entity.getComponent<HierarchyComponent>().parent == entt::null);
			}

			if (isRoot) {
				// Set parent without adjusting local transform (we want them relative to new root)
				TransformUtils::setParent(_scene->getRegistry(), entity.getHandle(), modelRoot.getHandle(), false);
			}
		}
	}

	// Notify renderer that scene geometry changed
	_renderer.markSceneDirty();

	// Load second instance of the same model at a different position
	auto result2 = ModelLoader::loadModelIntoScene(modelPath, *_scene, _resourceManager);
	if (result2) {
		// Create a root entity for the helmet model
		Entity helmetRoot = _scene->createEntity("HelmetRoot");
		auto& rootTransform = helmetRoot.getComponent<TransformComponent>();
		rootTransform.setPosition(glm::vec3(0.0f, 1.5f, 0.0f));

		// Reparent all loaded ROOT entities to the helmet root
		for (auto& entity : result2.value()) {
			bool isRoot = true;
			if (entity.hasComponent<HierarchyComponent>()) {
				isRoot = (entity.getComponent<HierarchyComponent>().parent == entt::null);
			}

			if (isRoot) {
				TransformUtils::setParent(_scene->getRegistry(), entity.getHandle(), helmetRoot.getHandle(), false);
			}
		}

		// Notify renderer again
		_renderer.markSceneDirty();
	}

	// Create camera entity
	Entity cameraEntity = _scene->createEntity("MainCamera");
	auto& cam = cameraEntity.addComponent<CameraComponent>();
	cam.primary = true;
	cam.fov = 45.0f;
	cam.nearClip = 0.1f;
	cam.farClip = 30.0f;

	auto& orbit = cameraEntity.addComponent<OrbitControllerComponent>();
	orbit.target = glm::vec3(0.0f);
	orbit.distance = 5.0f;

	// Position camera back so we can see the scene
	auto& camTransform = cameraEntity.getComponent<TransformComponent>();
	camTransform.position = glm::vec3(0.0f, 0.0f, 5.0f);
	camTransform.dirty = true;

	FISHY_LOG_TRACE("Camera entity created");

	// Create a directional light entity
	Entity lightEntity = _scene->createEntity("DirectionalLight");
	auto& light = lightEntity.addComponent<LightComponent>();
	light.type = LightType::Directional;
	light.color = glm::vec3(1.0f, 1.0f, 1.0f); // Warm white
	light.intensity = 5.0f;

	// Set light direction via transform rotation
	auto& lightTransform = lightEntity.getComponent<TransformComponent>();
	lightTransform.setRotationEuler(glm::vec3(glm::radians(-45.0f), glm::radians(45.0f), 0.0f));
	FISHY_LOG_TRACE("Light entity created");

	// Load IBL environment
	try {
		_iblEnvironment = IBLEnvironment::load(_device, "assets/environments/ennis");
		_renderer.setIBLEnvironment(_iblEnvironment.get());
	} catch (const std::exception& e) {
		FISHY_LOG_WARN("Failed to load IBL environment: {}", e.what());
	}

	FISHY_LOG_INFO("Application initialization complete");
}

Application::~Application() {
	FISHY_LOG_TRACE("Shutting down Application...");
	// Wait for GPU before destroying resources
	_device->waitIdle();
	_imguiLayer.reset();
	_scene.reset();
	FISHY_LOG_TRACE("Application shutdown complete");
}

// Main application loop
void Application::run() {
	FISHY_LOG_INFO("Starting main render loop");
	DebugPanel debugPanel;

	// Frame timing
	auto lastFrameTime = std::chrono::high_resolution_clock::now();

	while (!_window.shouldClose()) {
		// Calculate delta time
		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
		lastFrameTime = currentTime;

		_window.update();

		// Process camera input (only if ImGui is not capturing mouse)
		ImGuiIO& io = ImGui::GetIO();
		if (!io.WantCaptureMouse) {
			const auto& mouseState = _window.getMouseState();
			_orbitControllerSystem.processInput(
				*_scene, static_cast<float>(mouseState.deltaX), static_cast<float>(mouseState.deltaY),
				static_cast<float>(mouseState.scrollDelta), mouseState.rightButton, mouseState.middleButton);
		}

		// Reset mouse delta for next frame
		_window.resetMouseDelta();

		// Update ECS Systems
		_orbitControllerSystem.update(*_scene);
		_transformSystem.update(*_scene);
		_cameraSystem.update(*_scene, _renderer.getAspectRatio());
		_lightingSystem.update(*_scene);

		// Build RenderParams from system data
		RenderParams renderParams;
		if (auto camData = _cameraSystem.getPrimaryCameraData(*_scene)) {
			const auto& [viewMat, projMat, camPos] = *camData;
			renderParams.viewMatrix = viewMat;
			renderParams.projectionMatrix = projMat;
			renderParams.cameraPosition = camPos;
		} else {
			renderParams.viewMatrix = glm::mat4(1.0f);
			renderParams.projectionMatrix = glm::mat4(1.0f);
			renderParams.cameraPosition = glm::vec3(0.0f);
		}

		// Use first light from LightingSystem if available
		const auto& lights = _lightingSystem.getLightData();
		if (!lights.empty()) {
			const auto& light = lights[0];
			renderParams.lightDirection = glm::vec3(light.directionAndRange);
			renderParams.lightColor = glm::vec3(light.colorAndIntensity) * light.colorAndIntensity.w;
		}

		// Start ImGui frame
		_imguiLayer->newFrame();

		// Build UI
		debugPanel.draw();

		// Update debug settings in renderer
		const auto& settings = debugPanel.getSettings();
		_renderer.setDebugSettings(static_cast<float>(settings.debugViewInputs),
								   static_cast<float>(settings.debugViewEquation));

		// Render scene via RenderSystem with params
		_renderSystem.render(*_scene, _renderer, renderParams,
							 [this](VkCommandBuffer cmd) { _imguiLayer->render(cmd); });
	}

	FISHY_LOG_TRACE("Main render loop ended");
	_device->waitIdle();
}

void Application::createDefaultScene() {
	// Create a simple default scene with two quads
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

	// Use ResourceManager to create cached mesh and material
	auto meshHandle = _resourceManager.createMesh(entt::hashed_string{"__default_quad_mesh__"}, vertices, indices);
	auto materialHandle = _resourceManager.createMaterial(entt::hashed_string{"__default_material__"});

	// Pre-compute bindless texture indices for default material
	_resourceManager.populateMaterialTextureIndices(*materialHandle);

	// Create entity with mesh and renderer components
	Entity entity = _scene->createEntity("DefaultQuad");
	entity.addComponent<MeshComponent>(meshHandle);
	entity.addComponent<MeshRendererComponent>(materialHandle);
}

} // namespace Fishy
