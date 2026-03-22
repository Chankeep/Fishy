#pragma once

#include <cstdint>
#include <volk.h>

#include "../resources/IBLEnvironment.h"
#include "../resources/Mesh.h"
#include "../resources/MeshGenerator.h"
#include "DrawTypes.h"
#include "GraphicsPipeline.h"
#include "PipelineManager.h"
#include "RenderGraph.h"
#include "core/SwapChain.h"
#include "core/VulkanBuffer.h"
#include "RenderTexture.h"
#include "passes/UIPass.h"

#include <functional>
#include <memory>
#include <vector>

#include "RenderConstants.h"

namespace Fishy {

class VulkanDevice;
class Window;
class SwapChain;
class ResourceManager;
class CommandPool;
class Scene;
class UIPass;
class SceneFramebuffer;
struct RenderParams;

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
	 * @brief Render scene to offscreen SceneFramebuffer (shadows + main + skybox).
	 * Call beginFrame() is handled internally. Does NOT present — call renderUI() after.
	 */
	void renderToTexture(Scene& scene, const RenderParams& params, SceneFramebuffer& target);

	/**
	 * @brief Render UI (ImGui) to swapchain and present.
	 * Must be called after renderToTexture() within the same frame.
	 */
	void renderUI(std::function<void(VkCommandBuffer)> uiCallback);

	// Debug settings
	void setDebugSettings(float debugViewInputs, float debugViewEquation) {
		_debugViewInputs = debugViewInputs;
		_debugViewEquation = debugViewEquation;
	}

	/**
	 * @brief Mark scene geometry as dirty, forcing a rebuild of unified buffers.
	 * Call this after adding or removing entities with MeshComponents.
	 */
	void markSceneDirty() { _unifiedBuffersDirty = true; }

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
	void prepareSwapchainForRendering(const vk::raii::CommandBuffer& cmd);
	void prepareSwapchainForPresent(const vk::raii::CommandBuffer& cmd);
	void prepareOffscreenForRendering(const vk::raii::CommandBuffer& cmd, SceneFramebuffer& target);
	void prepareOffscreenForSampling(const vk::raii::CommandBuffer& cmd, SceneFramebuffer& target);
	void updateUniformBuffer(uint32_t frameIndex);
	void updateLightBuffer();
	void updateShadowData();
	[[nodiscard]] glm::mat4 computeFrustumLightSpaceMatrix(const glm::vec3& lightDir,
														   const std::array<glm::vec3, 8>& frustumCorners) const;
	void buildSceneData(Scene& scene);
	[[nodiscard]] RenderGraphContext createRenderContext(const RenderParams& params);

	// Initialization
	void createCommandBuffers();
	void createSyncObjects();
	void freeCommandBuffers();
	void recreateSwapChain();
	void createPipelines();
	void registerShadowMapPass();
	void registerSkyboxPass();
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
	void buildDrawBatches(Scene& scene);
	void buildInstanceData(Scene& scene);

	void updateInstanceDataBuffer();
	[[nodiscard]] vk::Format findDepthFormat();
	void writeTextureDescriptors();
	struct FrameData {
		vk::raii::CommandBuffer commandBuffer = nullptr;
		vk::raii::Semaphore imageAvailableSemaphore = nullptr;
		vk::raii::Fence inFlightFence = nullptr;
		std::unique_ptr<VulkanBuffer> uniformBuffer;
		vk::raii::DescriptorSet descriptorSet = nullptr; // Set 0: IBL textures + shadow map
		// Indirect draw buffer
		std::unique_ptr<VulkanBuffer> indirectBuffer;
		// Instance data SSBO for bindless rendering
		std::unique_ptr<VulkanBuffer> instanceDataBuffer;
		// Light data SSBO for multi-lighting
		std::unique_ptr<VulkanBuffer> lightDataBuffer;
		// Shadow data SSBO for multi-light shadows
		std::unique_ptr<VulkanBuffer> shadowDataBuffer;
	};

	VulkanDevice& _device;
	Window& _window;
	ResourceManager& _resourceManager;

	std::unique_ptr<SwapChain> _swapChain;

	// Depth resources
	std::unique_ptr<RenderTexture> _depthImage;
	vk::Format _depthFormat = vk::Format::eUndefined;

	// SwapChain image views
	std::vector<vk::raii::ImageView> _swapChainImageViews;

	// Sync objects
	std::vector<vk::raii::Semaphore> _renderFinishedSemaphores;

	// Pipeline Manager (owns all cached pipelines)
	std::unique_ptr<PipelineManager> _pipelineManager;

	// Render Graph (manages render passes)
	RenderGraph _renderGraph;

	// UI Pass (managed separately, has its own beginRendering/endRendering)
	std::unique_ptr<UIPass> _uiPass;

	// Current pipeline pointers (owned by _pipelineManager)
	GraphicsPipeline* _shadowPipeline = nullptr;
	GraphicsPipeline* _graphicsPipeline = nullptr;
	GraphicsPipeline* _skyboxPipeline = nullptr;

	// Descriptor Set Layouts
	vk::raii::DescriptorSetLayout _IBLSetLayout = nullptr;			   // Set 0: IBL textures
	vk::raii::DescriptorSetLayout _bindlessTextureSetLayout = nullptr; // Set 1: Bindless Textures

	// Descriptor Pools
	vk::raii::DescriptorPool _descriptorPool = nullptr;			// For global sets
	vk::raii::DescriptorPool _bindlessDescriptorPool = nullptr; // For bindless (UPDATE_AFTER_BIND)

	// Bindless descriptor sets (global, persistent)
	vk::raii::DescriptorSet _bindlessTextureSet = nullptr;
	vk::raii::DescriptorSet _bindlessStorageBufferSet = nullptr;

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
	uint32_t _shadowCasterCount = 0;          // Number of shadow-casting lights this frame

	// Unified geometry buffers (rebuilt when model changes)
	std::unique_ptr<VulkanBuffer> _unifiedVertexBuffer;
	std::unique_ptr<VulkanBuffer> _unifiedIndexBuffer;
	bool _unifiedBuffersDirty = true;

	// IBL environment (externally owned)
	IBLEnvironment* _iblEnvironment = nullptr;

	// Current frame render params (pointer to data passed by RenderSystem, valid only during renderScene)
	const RenderParams* _currentRenderParams = nullptr;
};

} // namespace Fishy
