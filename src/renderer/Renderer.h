#pragma once

#include <volk.h>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include "../resources/Mesh.h"
#include "../resources/Model.h"
#include "Camera.h"
#include "GraphicsPipeline.h"
#include "core/SwapChain.h"
#include "core/VulkanBuffer.h"

#include <functional>
#include <memory>
#include <vector>

namespace Fishy {

class VulkanDevice;
class Window;
class SwapChain;
class ResourceManager;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

/**
 * @brief Main renderer class.
 *
 * Handles all rendering operations including frame management, pipeline binding, and draw calls.
 * GPU resource management (meshes, materials) is delegated to ResourceManager.
 */
class Renderer {
public:
	Renderer(VulkanDevice& device, Window& window, ResourceManager& resourceManager);
	~Renderer();

	/**
	 * @brief Render a model with optional UI callback.
	 *
	 * This is the main rendering interface. All command buffer recording is encapsulated here.
	 * The uiRenderCallback is called after scene rendering but before present, allowing ImGui to render.
	 *
	 * @param model The model to render.
	 * @param uiRenderCallback Optional callback for UI rendering (receives VkCommandBuffer).
	 */
	void render(const Model& model, std::function<void(VkCommandBuffer)> uiRenderCallback = nullptr);

	// Debug settings
	void setDebugSettings(float debugViewInputs, float debugViewEquation) {
		_debugViewInputs = debugViewInputs;
		_debugViewEquation = debugViewEquation;
	}

	// Accessors
	float getAspectRatio() const;
	vk::Format getSwapChainFormat() const { return _swapChain->getFormat(); }
	vk::Extent2D getSwapChainExtent() const { return _swapChain->getExtent(); }

	// Camera access
	Camera& getCamera() { return _camera; }
	const Camera& getCamera() const { return _camera; }

private:
	// Frame management
	bool beginFrame();
	void endFrame();

	// Rendering helpers
	void updateUniformBuffer(uint32_t frameIndex);
	static void transitionImage(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout oldLayout,
								vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask,
								vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask,
								vk::PipelineStageFlags2 dstStageMask, vk::ImageAspectFlags aspectMask);

	// Initialization
	void createCommandBuffers();
	void createSyncObjects();
	void freeCommandBuffers();
	void recreateSwapChain();
	void createGraphicsPipeline();
	void createUniformBuffers();
	void createGlobalSetLayout();
	void createMaterialSetLayout();
	void createDescriptorPool();
	void createDescriptorSets();
	void createDepthResources();
	vk::Format findDepthFormat();
	void createSwapChainImageViews();

	struct FrameData {
		vk::raii::CommandBuffer commandBuffer = nullptr;
		vk::raii::Semaphore imageAvailableSemaphore = nullptr;
		vk::raii::Fence inFlightFence = nullptr;
		std::unique_ptr<VulkanBuffer> uniformBuffer;
		vk::raii::DescriptorSet descriptorSet = nullptr;
	};

private:
	VulkanDevice& _device;
	Window& _window;
	ResourceManager& _resourceManager;

	// Camera for orbit controls
	Camera _camera;

	std::unique_ptr<SwapChain> _swapChain;

	// Depth resources
	vk::raii::Image _depthImage = nullptr;
	vk::raii::DeviceMemory _depthImageMemory = nullptr;
	vk::raii::ImageView _depthImageView = nullptr;
	vk::Format _depthFormat = vk::Format::eUndefined;

	// SwapChain image views
	std::vector<vk::raii::ImageView> _swapChainImageViews;

	// Sync objects
	std::vector<vk::raii::Semaphore> _renderFinishedSemaphores;

	// Command Pool
	vk::raii::CommandPool _commandPool = nullptr;

	// Graphics Pipeline
	std::unique_ptr<GraphicsPipeline> _graphicsPipeline;
	vk::raii::DescriptorSetLayout _globalSetLayout = nullptr;
	vk::raii::DescriptorSetLayout _materialSetLayout = nullptr;

	// Descriptor Pool
	vk::raii::DescriptorPool _descriptorPool = nullptr;

	// Frame Data
	std::vector<FrameData> _frames;

	// Frame state
	uint32_t _currentImageIndex = 0;
	int _currentFrameIndex = 0;
	bool _isFrameStarted = false;

	// Debug settings
	float _debugViewInputs = 0.0f;
	float _debugViewEquation = 0.0f;
};

} // namespace Fishy