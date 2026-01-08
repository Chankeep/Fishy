#pragma once

#include <cstdint>
#include <volk.h>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include "../resources/IBLEnvironment.h"
#include "../resources/Mesh.h"
#include "../resources/MeshGenerator.h"
#include "../resources/Model.h"
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
class Scene;
struct RenderParams;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// Initial buffer sizes for indirect rendering
static constexpr uint32_t INITIAL_MAX_OBJECTS = 256;
static constexpr uint32_t INITIAL_MAX_COMMANDS = 256;

// Bindless texture array capacity
static constexpr uint32_t MAX_BINDLESS_TEXTURES = 4096;

struct PushConstants {
	uint64_t instanceDataAddress;
	uint64_t globalDataAddress;
};

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
	 * @brief Render a scene with optional UI callback.
	 *
	 * ECS-based rendering interface. Iterates entities with MeshComponent and MeshRendererComponent.
	 *
	 * @param scene The scene containing entities to render.
	 * @param params Camera and light data for this frame.
	 * @param uiRenderCallback Optional callback for UI rendering.
	 */
	void renderScene(Scene& scene, const RenderParams& params,
					 std::function<void(VkCommandBuffer)> uiRenderCallback = nullptr);

	// Debug settings
	void setDebugSettings(float debugViewInputs, float debugViewEquation) {
		_debugViewInputs = debugViewInputs;
		_debugViewEquation = debugViewEquation;
	}

	// Accessors
	[[nodiscard]] float getAspectRatio() const;
	[[nodiscard]] vk::Format getSwapChainFormat() const { return _swapChain->getFormat(); }
	[[nodiscard]] vk::Extent2D getSwapChainExtent() const { return _swapChain->getExtent(); }

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
	void createSkyboxPipeline();
	void createSkyboxMesh();
	void createUniformBuffers();
	void createSetLayout();
	void createBindlessTextureSetLayout();
	void createBindlessDescriptorPool();
	void createDescriptorPool();
	void allocateDescriptorSets();
	void createDepthResources();
	void createIndirectBuffer();
	void createSwapChainImageViews();

	void buildUnifiedBuffersFromScene(Scene& scene);
	void buildDrawBatchesFromScene(Scene& scene);
	void buildInstanceDataFromScene(Scene& scene);

	void updateInstanceDataBuffer();
	void renderIndirect(const vk::raii::CommandBuffer& cmd);
	void renderSkybox(const vk::raii::CommandBuffer& cmd);
	[[nodiscard]] vk::Format findDepthFormat();
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

	// Graphics Pipeline
	std::unique_ptr<GraphicsPipeline> _graphicsPipeline;
	std::unique_ptr<GraphicsPipeline> _skyboxPipeline;

	// Descriptor Set Layouts
	vk::raii::DescriptorSetLayout _IBLSetLayout = nullptr;			   // Set 0: IBL textures
	vk::raii::DescriptorSetLayout _bindlessTextureSetLayout = nullptr; // Set 1: Bindless Textures

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

	// skybox geometry buffers
	std::unique_ptr<VulkanBuffer> _skyboxVertexBuffer;
	std::unique_ptr<VulkanBuffer> _skyboxIndexBuffer;
	uint32_t _skyboxIndexCount;

	// IBL environment (externally owned)
	IBLEnvironment* _iblEnvironment = nullptr;

	// Current frame render params (pointer to data passed by RenderSystem, valid only during renderScene)
	const RenderParams* _currentRenderParams = nullptr;
};

} // namespace Fishy
