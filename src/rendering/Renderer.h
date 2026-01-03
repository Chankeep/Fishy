#pragma once

#include <volk.h>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include "../resources/IBLEnvironment.h"
#include "../resources/Mesh.h"
#include "../resources/Model.h"
#include "Camera.h"
#include "DrawTypes.h"
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
class CommandPool;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// Initial buffer sizes for indirect rendering
static constexpr uint32_t INITIAL_MAX_OBJECTS = 256;
static constexpr uint32_t INITIAL_MAX_COMMANDS = 256;

// Bindless texture array capacity
static constexpr uint32_t MAX_BINDLESS_TEXTURES = 4096;

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
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

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
	[[nodiscard]] float getAspectRatio() const;
	[[nodiscard]] vk::Format getSwapChainFormat() const { return _swapChain->getFormat(); }
	[[nodiscard]] vk::Extent2D getSwapChainExtent() const { return _swapChain->getExtent(); }

	// Camera access
	[[nodiscard]] Camera& getCamera() { return _camera; }
	[[nodiscard]] const Camera& getCamera() const { return _camera; }

	// IBL environment
	void setIBLEnvironment(IBLEnvironment* ibl);
	[[nodiscard]] IBLEnvironment* getIBLEnvironment() const { return _iblEnvironment; }

private:
	// Frame management
	bool beginFrame();
	void endFrame();

	// Rendering helpers
	void updateUniformBuffer(uint32_t frameIndex);

	// Initialization
	void createCommandBuffers();
	void createSyncObjects();
	void freeCommandBuffers();
	void recreateSwapChain();
	void createGraphicsPipeline();
	void createUniformBuffers();
	void createGlobalSetLayout();
	void createBindlessTextureSetLayout();
	void createBindlessDescriptorPool();
	void allocateBindlessDescriptorSet();
	void createDescriptorPool();
	void createDescriptorSets();
	void createGlobalDescriptorSets();
	void createDepthResources();
	void createIndirectBuffer();
	void buildUnifiedBuffers(const Model& model);
	void buildDrawBatches(const Model& model);
	void buildInstanceData(const Model& model);
	void updateInstanceDataBuffer();
	void renderIndirect(const vk::raii::CommandBuffer& cmd);
	[[nodiscard]] vk::Format findDepthFormat();
	void createSwapChainImageViews();
	void writeIBLDescriptors();

	struct FrameData {
		vk::raii::CommandBuffer commandBuffer = nullptr;
		vk::raii::Semaphore imageAvailableSemaphore = nullptr;
		vk::raii::Fence inFlightFence = nullptr;
		std::unique_ptr<VulkanBuffer> uniformBuffer;
		vk::raii::DescriptorSet descriptorSet = nullptr; // Global UBO set
		// Indirect draw buffer
		std::unique_ptr<VulkanBuffer> indirectBuffer;
		// Instance data SSBO for bindless rendering
		std::unique_ptr<VulkanBuffer> instanceDataBuffer;
	};

	VulkanDevice& _device;
	Window& _window;
	ResourceManager& _resourceManager;

	// Camera for orbit controls
	Camera _camera;

	std::unique_ptr<SwapChain> _swapChain;

	// Depth resources
	VkImage _depthImage = VK_NULL_HANDLE;
	VmaAllocation _depthAllocation = nullptr;
	vk::raii::ImageView _depthImageView = nullptr;
	vk::Format _depthFormat = vk::Format::eUndefined;

	// SwapChain image views
	std::vector<vk::raii::ImageView> _swapChainImageViews;

	// Sync objects
	std::vector<vk::raii::Semaphore> _renderFinishedSemaphores;

	// Command Pool (borrowed from VulkanDevice)
	CommandPool* _commandPool = nullptr;

	// Graphics Pipeline
	std::unique_ptr<GraphicsPipeline> _graphicsPipeline;

	// Descriptor Set Layouts
	vk::raii::DescriptorSetLayout _globalSetLayout = nullptr;				 // Set 0: Global Data
	vk::raii::DescriptorSetLayout _bindlessTextureSetLayout = nullptr;		 // Set 1: Bindless Textures
	vk::raii::DescriptorSetLayout _bindlessStorageBufferSetLayout = nullptr; // Set 2: Bindless SSBOs

	// Descriptor Pools
	vk::raii::DescriptorPool _descriptorPool = nullptr;			// For global sets
	vk::raii::DescriptorPool _bindlessDescriptorPool = nullptr; // For bindless (UPDATE_AFTER_BIND)

	// Bindless descriptor sets (global, persistent)
	vk::raii::DescriptorSet _bindlessTextureSet = nullptr;
	vk::raii::DescriptorSet _bindlessStorageBufferSet = nullptr;

	// Pipeline Cache (disk-persistent)
	vk::raii::PipelineCache _pipelineCache = nullptr;
	static constexpr const char* PIPELINE_CACHE_FILENAME = "pipeline_cache.bin";
	void loadPipelineCache();
	void savePipelineCache();

	// Frame Data
	std::vector<FrameData> _frames;

	// Frame state
	uint32_t _currentImageIndex = 0;
	int _currentFrameIndex = 0;
	bool _isFrameStarted = false;

	// Debug settings
	float _debugViewInputs = 0.0f;
	float _debugViewEquation = 0.0f;

	// Indirect rendering state (rebuilt each frame)
	std::vector<DrawBatch> _drawBatches;
	std::vector<vk::DrawIndexedIndirectCommand> _indirectCommands;
	std::vector<MeshRegion> _meshRegions;	 // Per-primitive offsets in unified buffers
	std::vector<InstanceData> _instanceData; // CPU-side instance data for SSBO upload

	// Unified geometry buffers (rebuilt when model changes)
	std::unique_ptr<VulkanBuffer> _unifiedVertexBuffer;
	std::unique_ptr<VulkanBuffer> _unifiedIndexBuffer;
	bool _unifiedBuffersDirty = true;

	// IBL environment (externally owned)
	IBLEnvironment* _iblEnvironment = nullptr;
};

} // namespace Fishy