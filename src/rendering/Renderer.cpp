#include "Renderer.h"

#include <array>
#include <limits>

#include "PipelineBuilder.h"
#include "RenderConstants.h"
#include "core/CommandPool.h"
#include "core/LogSystem.h"
#include "core/VulkanDevice.h"
#include "core/VulkanImage.h"
#include "core/VulkanUtils.h"
#include "RenderTexture.h"
#include "SceneFramebuffer.h"
#include "core/Window.h"
#include "ecs/components/LightComponent.h"
#include "ecs/components/MeshComponent.h"
#include "ecs/components/MeshRendererComponent.h"
#include "ecs/components/TransformComponent.h"
#include "ecs/systems/LightingSystem.h"
#include "ecs/systems/RenderSystem.h" // For RenderParams
#include "passes/MainRenderPass.h"
#include "passes/ShadowMapPass.h"
#include "passes/SkyboxPass.h"
#include "passes/UIPass.h"
#include "resources/MeshGenerator.h"
#include "resources/ResourceManager.h"
#include "scene/Scene.h"
#include "vulkan/vulkan.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <stdexcept>

namespace Fishy {

static constexpr uint64_t FENCE_TIMEOUT = std::numeric_limits<uint32_t>::max();

Renderer::Renderer(VulkanDevice& device, Window& window, ResourceManager& resourceManager)
	: _device(device), _window(window), _resourceManager(resourceManager) {
	FISHY_LOG_INFO("Initializing Renderer...");
	_frames.resize(MAX_FRAMES_IN_FLIGHT);
	recreateSwapChain();
	createCommandBuffers();

	// Descriptor set layouts
	createSetLayout();
	createBindlessTextureSetLayout(); // Creates both texture and SSBO bindless layouts

	// Descriptor pools
	createDescriptorPool();			// For global sets
	createBindlessDescriptorPool(); // For bindless sets (UPDATE_AFTER_BIND)

	// Pipeline Manager (handles cache loading automatically)
	_pipelineManager = std::make_unique<PipelineManager>(_device);
	createPipelines();

	createUniformBuffers();
	createIndirectBuffer();

	// Allocate all descriptor sets (global + bindless) and write initial data
	allocateDescriptorSets();

	// Initialize ResourceManager with bindless sets (enables auto-registration)
	_resourceManager.initBindlessResources(*_bindlessTextureSet, *_bindlessStorageBufferSet);

	createSyncObjects();

	// Register render passes with their dependencies
	registerShadowMapPass();
	_renderGraph.addPass<MainRenderPass>(*_graphicsPipeline);
	registerSkyboxPass();

	// Note: UIPass is NOT in RenderGraph - it's managed separately
	// because it requires its own beginRendering/endRendering
	_uiPass = std::make_unique<UIPass>();

	FISHY_LOG_INFO("Renderer initialized successfully with {} render passes", _renderGraph.getPassCount());
}

Renderer::~Renderer() {
	FISHY_LOG_INFO("Shutting down Renderer...");

	// PipelineManager destructor will save cache automatically

	_device->waitIdle();

	freeCommandBuffers();
	_swapChain.reset();

	_unifiedVertexBuffer.reset();
	_unifiedIndexBuffer.reset();
}



void Renderer::renderToTexture(Scene& scene, const RenderParams& params, SceneFramebuffer& target) {
	if (!beginFrame()) {
		return;
	}

	// Update per-frame GPU data
	_currentRenderParams = &params;
	updateUniformBuffer(_currentFrameIndex);
	updateShadowData();
	updateLightBuffer();

	// Build scene data for rendering
	buildSceneData(scene);

	// Transition offscreen color: ready for rendering
	const auto& cmd = _frames[_currentFrameIndex].commandBuffer;
	prepareOffscreenForRendering(cmd, target);

	// Create render context targeting offscreen framebuffer
	RenderGraphContext ctx = createRenderContext(params);
	ctx.viewportExtent = target.getExtent();
	ctx.colorAttachmentView = target.getColorTexture().getImageView();
	ctx.depthAttachmentView = target.getDepthTexture().getImageView();

	// Execute render graph (ShadowMap -> Main -> Skybox) to offscreen
	_renderGraph.execute(ctx, scene.getRegistry());

	// Transition offscreen color: ready for ImGui sampling
	prepareOffscreenForSampling(cmd, target);

	// NOTE: do NOT endFrame() here - renderUI() will handle that
}

void Renderer::renderUI(std::function<void(VkCommandBuffer)> uiCallback) {
	if (!_isFrameStarted) {
		return;
	}

	const auto& cmd = _frames[_currentFrameIndex].commandBuffer;

	// Transition swapchain for UI rendering
	prepareSwapchainForRendering(cmd);

	// Execute UI pass on swapchain
	if (_uiPass && uiCallback) {
		_uiPass->setCallback(std::move(uiCallback));
		_uiPass->setImageView(*_swapChainImageViews[_currentImageIndex]);

		RenderGraphContext uiCtx = createRenderContext(*_currentRenderParams);
		uiCtx.viewportExtent = _swapChain->getExtent();
		uiCtx.colorAttachmentView = *_swapChainImageViews[_currentImageIndex];

		entt::registry dummyRegistry;
		_uiPass->execute(uiCtx, dummyRegistry);
	}

	// Prepare swapchain for presentation
	prepareSwapchainForPresent(cmd);

	endFrame();
}

void Renderer::prepareSwapchainForRendering(const vk::raii::CommandBuffer& cmd) {
	VulkanUtils::transitionImage(*cmd, _swapChain->getImages()[_currentImageIndex], vk::ImageLayout::eUndefined,
								 vk::ImageLayout::eColorAttachmentOptimal, vk::AccessFlagBits2::eNone,
								 vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eTopOfPipe,
								 vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor);
}

void Renderer::prepareSwapchainForPresent(const vk::raii::CommandBuffer& cmd) {
	VulkanUtils::transitionImage(*cmd, _swapChain->getImages()[_currentImageIndex],
								 vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
								 vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eNone,
								 vk::PipelineStageFlagBits2::eColorAttachmentOutput,
								 vk::PipelineStageFlagBits2::eBottomOfPipe, vk::ImageAspectFlagBits::eColor);
}

void Renderer::prepareOffscreenForRendering(const vk::raii::CommandBuffer& cmd, SceneFramebuffer& target) {
	VulkanUtils::transitionImage(*cmd, target.getColorTexture().getImage(), vk::ImageLayout::eUndefined,
								 vk::ImageLayout::eColorAttachmentOptimal, vk::AccessFlagBits2::eNone,
								 vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eTopOfPipe,
								 vk::PipelineStageFlagBits2::eColorAttachmentOutput,
								 vk::ImageAspectFlagBits::eColor);
}

void Renderer::prepareOffscreenForSampling(const vk::raii::CommandBuffer& cmd, SceneFramebuffer& target) {
	VulkanUtils::transitionImage(*cmd, target.getColorTexture().getImage(),
								 vk::ImageLayout::eColorAttachmentOptimal,
								 vk::ImageLayout::eShaderReadOnlyOptimal,
								 vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eShaderRead,
								 vk::PipelineStageFlagBits2::eColorAttachmentOutput,
								 vk::PipelineStageFlagBits2::eFragmentShader,
								 vk::ImageAspectFlagBits::eColor);
}

void Renderer::buildSceneData(Scene& scene) {
	buildInstanceData(scene);
	updateInstanceDataBuffer();
	buildDrawBatches(scene);
}

RenderGraphContext Renderer::createRenderContext(const RenderParams& params) {
	vk::Extent2D extent = _swapChain->getExtent();
	return RenderGraphContext{
		.cmd = _frames[_currentFrameIndex].commandBuffer,
		.viewportExtent = extent,
		.params = params,
		.globalDescriptorSet = *_frames[_currentFrameIndex].descriptorSet,
		.bindlessTextureSet = *_bindlessTextureSet,
		.instanceDataAddress = _frames[_currentFrameIndex].instanceDataBuffer->getDeviceAddress(),
		.globalDataAddress = _frames[_currentFrameIndex].uniformBuffer->getDeviceAddress(),
		.lightDataAddress = _frames[_currentFrameIndex].lightDataBuffer
								? _frames[_currentFrameIndex].lightDataBuffer->getDeviceAddress()
								: 0,
		.shadowDataAddress = _frames[_currentFrameIndex].shadowDataBuffer
								? _frames[_currentFrameIndex].shadowDataBuffer->getDeviceAddress()
								: 0,
		.shadowCasterCount = _shadowCasterCount,
		.drawBatches = _drawBatches,
		.indirectBuffer = _frames[_currentFrameIndex].indirectBuffer.get(),
		.vertexBuffer = _unifiedVertexBuffer.get(),
		.indexBuffer = _unifiedIndexBuffer.get(),
		.iblEnvironment = _iblEnvironment,
		.colorAttachmentView = *_swapChainImageViews[_currentImageIndex],
		.depthAttachmentView = _depthImage->getImageView(),
	};
}

void Renderer::recreateSwapChain() {
	auto extent = _window.getExtent();
	while (extent.width == 0 || extent.height == 0) {
		extent = _window.getExtent();
		glfwWaitEvents();
	}

	FISHY_LOG_INFO("Recreating swap chain: {}x{}", extent.width, extent.height);

	// wait for all in-flight frames to complete instead of full device idle
	std::vector<vk::Fence> fencesToWait;
	for (const auto& frame : _frames) {
		if (*frame.inFlightFence) {
			fencesToWait.push_back(*frame.inFlightFence);
		}
	}

	if (!fencesToWait.empty()) {
		auto result = _device->waitForFences(fencesToWait, vk::True, FENCE_TIMEOUT);
		if (result != vk::Result::eSuccess) {
			FISHY_LOG_ERROR("Failed to wait for in-flight fences during SwapChain recreation");
		}
	}

	if (_swapChain == nullptr) {
		_swapChain = std::make_unique<SwapChain>(_device, _window.getSurface(), extent.width, extent.height);
	} else {
		_swapChain->recreate(extent.width, extent.height);
	}

	createSwapChainImageViews();
	createDepthResources();
}

void Renderer::createCommandBuffers() {
	auto commandPool = &_device.getTransferCommandPool();

	vk::CommandBufferAllocateInfo allocInfo{.commandPool = *commandPool->getCommandPool(),
											.level = vk::CommandBufferLevel::ePrimary,
											.commandBufferCount = MAX_FRAMES_IN_FLIGHT};

	auto commandBuffers = vk::raii::CommandBuffers(*_device, allocInfo);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_frames[i].commandBuffer = std::move(commandBuffers[i]);
	}
}

void Renderer::freeCommandBuffers() {
	for (auto& frame : _frames) {
		frame.commandBuffer = nullptr;
	}
}

void Renderer::createSyncObjects() {
	// Frame-in-flight sync objects
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_frames[i].imageAvailableSemaphore = vk::raii::Semaphore(*_device, vk::SemaphoreCreateInfo{});
		_frames[i].inFlightFence =
			vk::raii::Fence(*_device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
	}

	// Per-swapchain-image sync objects
	_renderFinishedSemaphores.clear();
	size_t imageCount = _swapChain->getImageCount();
	for (size_t i = 0; i < imageCount; i++) {
		_renderFinishedSemaphores.emplace_back(*_device, vk::SemaphoreCreateInfo{});
	}
}

void Renderer::createSetLayout() {
	// Set 0: IBL and shadow map textures
	// Binding 0: irradianceMap (samplerCube)
	// Binding 1: prefilteredEnvMap (samplerCube)
	// Binding 2: brdfLUT (sampler2D)
	// Binding 3: shadow depth map (sampler2D)
	std::vector<vk::DescriptorSetLayoutBinding> bindings = {
		{.binding = 0,
		 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
		 .descriptorCount = 1,
		 .stageFlags = vk::ShaderStageFlagBits::eFragment},
		{.binding = 1,
		 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
		 .descriptorCount = 1,
		 .stageFlags = vk::ShaderStageFlagBits::eFragment},
		{.binding = 2,
		 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
		 .descriptorCount = 1,
		 .stageFlags = vk::ShaderStageFlagBits::eFragment},
		{.binding = 3,
		 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
		 .descriptorCount = 1,
		 .stageFlags = vk::ShaderStageFlagBits::eFragment},
	};

	vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = static_cast<uint32_t>(bindings.size()),
												 .pBindings = bindings.data()};

	_IBLSetLayout = vk::raii::DescriptorSetLayout(*_device, layoutInfo);
}

void Renderer::createBindlessTextureSetLayout() {
	FISHY_LOG_INFO("[Bindless] Creating texture and SSBO set layouts (max {} entries)", MAX_BINDLESS_TEXTURES);

	// Set 1: Bindless Texture Array
	vk::DescriptorBindingFlags textureBindingFlags =
		vk::DescriptorBindingFlagBits::eUpdateAfterBind |		 // Can update while bound to command buffer
		vk::DescriptorBindingFlagBits::ePartiallyBound |		 // Not all slots need valid descriptors
		vk::DescriptorBindingFlagBits::eVariableDescriptorCount; // Size set at allocation time

	vk::DescriptorSetLayoutBindingFlagsCreateInfo textureBindingFlagsInfo{.bindingCount = 1,
																		  .pBindingFlags = &textureBindingFlags};

	vk::DescriptorSetLayoutBinding textureBinding{.binding = 0,
												  .descriptorType = vk::DescriptorType::eCombinedImageSampler,
												  .descriptorCount = MAX_BINDLESS_TEXTURES,
												  .stageFlags = vk::ShaderStageFlagBits::eFragment};

	vk::StructureChain<vk::DescriptorSetLayoutCreateInfo, vk::DescriptorSetLayoutBindingFlagsCreateInfo> layoutChain{
		vk::DescriptorSetLayoutCreateInfo{.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
										  .bindingCount = 1,
										  .pBindings = &textureBinding},
		textureBindingFlagsInfo};

	_bindlessTextureSetLayout =
		vk::raii::DescriptorSetLayout(*_device, layoutChain.get<vk::DescriptorSetLayoutCreateInfo>());
	FISHY_LOG_INFO("[Bindless] Texture set layout created");
}

void Renderer::createBindlessDescriptorPool() {
	// Pool needs UPDATE_AFTER_BIND flag to match layout
	std::array<vk::DescriptorPoolSize, 2> poolSizes{
		vk::DescriptorPoolSize{.type = vk::DescriptorType::eCombinedImageSampler,
							   .descriptorCount = MAX_BINDLESS_TEXTURES},
		vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer, .descriptorCount = MAX_BINDLESS_TEXTURES}};

	vk::DescriptorPoolCreateInfo poolInfo{
		.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind |
				 vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, // Both flags required
		.maxSets = 2,												   // One texture set + one SSBO set
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()};

	_bindlessDescriptorPool = vk::raii::DescriptorPool(*_device, poolInfo);
	FISHY_LOG_INFO("[Bindless] Descriptor pool created with UPDATE_AFTER_BIND flag");
}

void Renderer::allocateDescriptorSets() {
	// Allocate Bindless Texture Set (Set 1)
	uint32_t textureVariableCount = MAX_BINDLESS_TEXTURES;

	vk::DescriptorSetVariableDescriptorCountAllocateInfo textureVariableInfo{
		.descriptorSetCount = 1, .pDescriptorCounts = &textureVariableCount};

	vk::StructureChain<vk::DescriptorSetAllocateInfo, vk::DescriptorSetVariableDescriptorCountAllocateInfo> allocChain{
		vk::DescriptorSetAllocateInfo{.descriptorPool = *_bindlessDescriptorPool,
									  .descriptorSetCount = 1,
									  .pSetLayouts = &*_bindlessTextureSetLayout},
		textureVariableInfo};

	auto textureSets = vk::raii::DescriptorSets(*_device, allocChain.get<vk::DescriptorSetAllocateInfo>());
	_bindlessTextureSet = std::move(textureSets[0]);
	FISHY_LOG_INFO("[Bindless] Texture descriptor set allocated");

	// Allocate Global Descriptor Sets (Set 0: IBL textures)
	std::vector<vk::DescriptorSetLayout> IBLLayouts(MAX_FRAMES_IN_FLIGHT, *_IBLSetLayout);

	vk::DescriptorSetAllocateInfo IBLAllocInfo{.descriptorPool = *_descriptorPool,
											   .descriptorSetCount = static_cast<uint32_t>(IBLLayouts.size()),
											   .pSetLayouts = IBLLayouts.data()};

	auto IBLSets = vk::raii::DescriptorSets(*_device, IBLAllocInfo);

	// Get default textures for placeholders
	auto defaultTex = _resourceManager.getDefaultWhiteTexture();
	auto defaultCubemap = _resourceManager.getDefaultCubemap();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_frames[i].descriptorSet = std::move(IBLSets[i]);

		// IBL placeholders (cubemaps)
		vk::DescriptorImageInfo cubemapImageInfo{
			.sampler = *defaultCubemap->getSampler(),
			.imageView = *defaultCubemap->getImageView(),
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		// BRDF LUT placeholder (2D texture)
		vk::DescriptorImageInfo brdfLutImageInfo{
			.sampler = *defaultTex->getSampler(),
			.imageView = *defaultTex->getImageView(),
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		std::array<vk::WriteDescriptorSet, 3> descriptorWrites{};

		// Binding 0: IBL irradiance map
		descriptorWrites[0].dstSet = *_frames[i].descriptorSet;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pImageInfo = &cubemapImageInfo;

		// Binding 1: IBL prefiltered map
		descriptorWrites[1].dstSet = *_frames[i].descriptorSet;
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &cubemapImageInfo;

		// Binding 2: BRDF LUT
		descriptorWrites[2].dstSet = *_frames[i].descriptorSet;
		descriptorWrites[2].dstBinding = 2;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].pImageInfo = &brdfLutImageInfo;

		_device->updateDescriptorSets(descriptorWrites, {});
	}
	FISHY_LOG_INFO("All descriptor sets allocated and initialized");
}

// NOTE: Object data is now managed via bindless SSBO array (Set 2)
// See BindlessResourceManager for runtime instance data management

void Renderer::createIndirectBuffer() {
	// Use header constant for initial buffer size
	vk::DeviceSize bufferSize = sizeof(vk::DrawIndexedIndirectCommand) * INITIAL_MAX_COMMANDS;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_frames[i].indirectBuffer = std::make_unique<VulkanBuffer>(
			_device, bufferSize, vk::BufferUsageFlagBits::eIndirectBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		_frames[i].indirectBuffer->map();

#ifndef NDEBUG
		std::string debugName = "IndirectBuffer_Frame" + std::to_string(i);
		VulkanUtils::setDebugName(_device, _frames[i].indirectBuffer->getBuffer(), debugName.c_str());
#endif
	}
	FISHY_LOG_TRACE("Created indirect draw buffers ({} bytes each)", bufferSize);
}

void Renderer::createPipelines() {
	FISHY_LOG_INFO("Creating graphics pipelines...");

	// Common setup
	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.data()};

	vk::PushConstantRange pushConstantRange{.stageFlags =
												vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
											.offset = 0,
											.size = sizeof(PushConstants)};

	// === Shadow Pipeline
	{
		{
			const auto& vertShader = _resourceManager.getShader("shaders/ShadowMap.slang", "vertMain");
			const auto& fragShader = _resourceManager.getShader("shaders/ShadowMap.slang", "fragMain");

			PipelineBuilder builder(**_device);
			builder.setShaderId(entt::hashed_string{"shaders/ShadowMap.slang"}.value())
				.setShaders(vertShader, fragShader, "main", "main")
				.setVertexInput(vertexInputInfo)
				.setInputTopology(vk::PrimitiveTopology::eTriangleList)
				.setCullMode(vk::CullModeFlagBits::eFront, vk::FrontFace::eCounterClockwise)
				.setLayout({}, {pushConstantRange})				 // No descriptor sets for shadow pass
				.setRenderingFormats({}, vk::Format::eD32Sfloat) // Depth-only, no color attachments
				.setDepthStencilTest(true, true, vk::CompareOp::eLess, false, vk::CompareOp::eAlways);

			_shadowPipeline = builder.build(*_pipelineManager);
		}
	}

	// === PBR Pipeline ===
	{
		const auto& vertShader = _resourceManager.getShader("shaders/PBRshader.slang", "vertMain");
		const auto& fragShader = _resourceManager.getShader("shaders/PBRshader.slang", "fragMain");

		PipelineBuilder builder(**_device);
		builder.setShaderId(entt::hashed_string{"shaders/PBRshader.slang"}.value())
			.setShaders(vertShader, fragShader, "main", "main")
			.setVertexInput(vertexInputInfo)
			.setInputTopology(vk::PrimitiveTopology::eTriangleList)
			.setCullMode(vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise)
			.setLayout({*_IBLSetLayout, *_bindlessTextureSetLayout}, {pushConstantRange})
			.setRenderingFormats({_swapChain->getFormat()}, _depthFormat)
			.setDepthStencilTest(true, true, vk::CompareOp::eLess, false, vk::CompareOp::eAlways);

		_graphicsPipeline = builder.build(*_pipelineManager);
	}

	// === Skybox Pipeline ===
	{
		const auto& vertShader = _resourceManager.getShader("shaders/Skybox.slang", "vertMain");
		const auto& fragShader = _resourceManager.getShader("shaders/Skybox.slang", "fragMain");

		PipelineBuilder builder(**_device);
		builder.setShaderId(entt::hashed_string{"shaders/Skybox.slang"}.value())
			.setShaders(vertShader, fragShader, "main", "main")
			.setVertexInput(vertexInputInfo)
			.setInputTopology(vk::PrimitiveTopology::eTriangleList)
			.setCullMode(vk::CullModeFlagBits::eFront, vk::FrontFace::eCounterClockwise)
			.setLayout({*_IBLSetLayout}, {pushConstantRange})
			.setRenderingFormats({_swapChain->getFormat()}, _depthFormat)
			.setDepthStencilTest(false, true, vk::CompareOp::eLessOrEqual, false, vk::CompareOp::eAlways);

		_skyboxPipeline = builder.build(*_pipelineManager);
	}

	FISHY_LOG_TRACE("All pipelines created successfully");
}

void Renderer::registerSkyboxPass() {
	FISHY_LOG_TRACE("Creating skybox mesh...");

	auto cubeMesh = MeshGenerator::createCube();
	const auto& vertices = cubeMesh->getVertices();
	const auto& indices = cubeMesh->getIndices();

	uint32_t indexCount = static_cast<uint32_t>(indices.size());

	vk::DeviceSize vertexBufferSize = sizeof(Vertex) * vertices.size();
	auto vertexBuffer = std::make_unique<VulkanBuffer>(
		_device, vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	VulkanBuffer stagingVertex(_device, vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
							   vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);

	stagingVertex.map();
	stagingVertex.upload(vertices.data(), vertexBufferSize);

	vk::DeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
	auto indexBuffer = std::make_unique<VulkanBuffer>(
		_device, indexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

#ifndef NDEBUG
	VulkanUtils::setDebugName(_device, vertexBuffer->getBuffer(), "Skybox_VertexBuffer");
	VulkanUtils::setDebugName(_device, indexBuffer->getBuffer(), "Skybox_IndexBuffer");
#endif

	auto stagingIndex =
		VulkanBuffer(_device, indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
					 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	stagingIndex.map();
	stagingIndex.upload(indices.data(), indexBufferSize);

	VulkanUtils::executeImmediate(_device, [&](auto& cmd) {
		cmd.copyBuffer(stagingVertex.getBuffer(), vertexBuffer->getBuffer(), vk::BufferCopy{.size = vertexBufferSize});
		cmd.copyBuffer(stagingIndex.getBuffer(), indexBuffer->getBuffer(), vk::BufferCopy{.size = indexBufferSize});
	});

	FISHY_LOG_TRACE("Skybox mesh created: {} vertices, {} indices", vertices.size(), indices.size());

	// Register SkyboxPass with ownership of buffers
	_renderGraph.addPass<SkyboxPass>(*_skyboxPipeline, std::move(vertexBuffer), std::move(indexBuffer), indexCount);
}

void Renderer::registerShadowMapPass() {
	FISHY_LOG_TRACE("Creating shadowMap image...");

	auto shadowMap = std::make_unique<RenderTexture>(
		_device, SHADOW_ATLAS_SIZE, SHADOW_ATLAS_SIZE, vk::Format::eD32Sfloat,
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled);

	FISHY_LOG_TRACE("Creating shadowMap sampler...");

	auto properties = _device.getPhysicalDevice().getProperties();
	vk::SamplerCreateInfo samplerInfo{
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = vk::SamplerAddressMode::eRepeat,
		.addressModeV = vk::SamplerAddressMode::eRepeat,
		.addressModeW = vk::SamplerAddressMode::eRepeat,
		.mipLodBias = 0.0f,
		.anisotropyEnable = vk::True,
		.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
		.compareEnable = vk::False,
		.compareOp = vk::CompareOp::eAlways,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = vk::BorderColor::eIntOpaqueBlack,
		.unnormalizedCoordinates = vk::False,
	};

	auto sampler = vk::raii::Sampler(*_device, samplerInfo);

	_renderGraph.addPass<ShadowMapPass>(*_shadowPipeline, std::move(shadowMap), std::move(sampler));

	FISHY_LOG_TRACE(" shadowMap image created: {}x{}", SHADOW_ATLAS_SIZE, SHADOW_ATLAS_SIZE);
}

void Renderer::createUniformBuffers() {
	vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_frames[i].uniformBuffer = std::make_unique<VulkanBuffer>(
			_device, bufferSize,
			vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		// Persistent mapping: map once and keep it mapped
		_frames[i].uniformBuffer->map();

#ifndef NDEBUG
		std::string debugName = "GlobalUBO_Frame" + std::to_string(i);
		VulkanUtils::setDebugName(_device, _frames[i].uniformBuffer->getBuffer(), debugName.c_str());
#endif
	}
}

void Renderer::createDescriptorPool() {
	std::array<vk::DescriptorPoolSize, 3> poolSizes{
		vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer,
							   .descriptorCount =
								   static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT + 100)}, // Extra for materials
		vk::DescriptorPoolSize{.type = vk::DescriptorType::eCombinedImageSampler,
							   .descriptorCount =
								   static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 13 + 500)}, // +3 for IBL per frame
		vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer,
							   .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)}}; // SSBO for object data

	vk::DescriptorPoolCreateInfo poolInfo{.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
										  .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2 + 100),
										  .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
										  .pPoolSizes = poolSizes.data()};

	_descriptorPool = vk::raii::DescriptorPool(*_device, poolInfo);
}

void Renderer::updateUniformBuffer(uint32_t frameIndex) {
	auto extent = _swapChain->getExtent();
	UniformBufferObject ubo{};

	if (_currentRenderParams) {
		ubo.view = _currentRenderParams->viewMatrix;
		ubo.proj = _currentRenderParams->projectionMatrix;
		ubo.camPos = glm::vec4(_currentRenderParams->cameraPosition, 0.0f);
		ubo.lightCount =
			_currentRenderParams->lights ? static_cast<uint32_t>(_currentRenderParams->lights->size()) : 0;
	} else {
		ubo.view = glm::mat4(1.0f);
		ubo.proj = glm::perspective(glm::radians(45.0f),
									static_cast<float>(extent.width) / static_cast<float>(extent.height), 0.1f, 100.0f);
		ubo.camPos = glm::vec4(0.0f, 0.0f, 3.0f, 0.0f);
		ubo.lightCount = 0;
	}

	// Debug settings
	ubo.debugViewInputs = _debugViewInputs;
	ubo.debugViewEquation = _debugViewEquation;

	// IBL parameters
	if (_iblEnvironment) {
		ubo.prefilteredMipLevels = static_cast<float>(_iblEnvironment->prefilteredMipLevels);
		ubo.iblIntensity = 0.5f;
	} else {
		ubo.prefilteredMipLevels = 1.0f;
		ubo.iblIntensity = 0.0f;
	}

	_frames[frameIndex].uniformBuffer->upload(&ubo, sizeof(ubo));
}

void Renderer::updateLightBuffer() {
	if (!_currentRenderParams || !_currentRenderParams->lights || _currentRenderParams->lights->empty()) {
		return;
	}

	auto& frame = _frames[_currentFrameIndex];
	const auto& lights = *_currentRenderParams->lights;
	vk::DeviceSize requiredSize = lights.size() * sizeof(LightData);

	if (!frame.lightDataBuffer || frame.lightDataBuffer->getSize() < requiredSize) {
		vk::DeviceSize allocSize = std::max(requiredSize, static_cast<vk::DeviceSize>(16 * sizeof(LightData)));

		frame.lightDataBuffer = std::make_unique<VulkanBuffer>(
			_device, allocSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		frame.lightDataBuffer->map();
	}
	frame.lightDataBuffer->upload(lights.data(), requiredSize);
}

glm::mat4 Renderer::computeFrustumLightSpaceMatrix(const glm::vec3& lightDir,
												   const std::array<glm::vec3, 8>& frustumCorners) const {
	// Compute frustum center
	glm::vec3 frustumCenter{0.0f};
	for (const auto& corner : frustumCorners) {
		frustumCenter += corner;
	}
	frustumCenter /= 8.0f;

	// Build light view matrix
	glm::vec3 lightPos = frustumCenter - lightDir;
	glm::mat4 lightView = glm::lookAt(lightPos, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));

	// Transform frustum corners to light space and compute AABB
	float minX = std::numeric_limits<float>::max();
	float maxX = std::numeric_limits<float>::lowest();
	float minY = std::numeric_limits<float>::max();
	float maxY = std::numeric_limits<float>::lowest();
	float minZ = std::numeric_limits<float>::max();
	float maxZ = std::numeric_limits<float>::lowest();

	for (const auto& corner : frustumCorners) {
		glm::vec4 lightSpacePos = lightView * glm::vec4(corner, 1.0f);
		minX = std::min(minX, lightSpacePos.x);
		maxX = std::max(maxX, lightSpacePos.x);
		minY = std::min(minY, lightSpacePos.y);
		maxY = std::max(maxY, lightSpacePos.y);
		minZ = std::min(minZ, lightSpacePos.z);
		maxZ = std::max(maxZ, lightSpacePos.z);
	}

	// Extend Z bounds to capture shadow casters behind the camera
	minZ -= 3.0f;

	// Minimal padding for tighter bounds
	float padding = 1.0f;
	minX -= padding;
	maxX += padding;
	minY -= padding;
	maxY += padding;

	// Build orthographic projection from AABB
	float nearPlane = -maxZ;
	float farPlane = -minZ;
	if (nearPlane >= farPlane) {
		nearPlane = 0.1f;
		farPlane = 200.0f;
	}
	glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, nearPlane, farPlane);
	lightProj[1][1] *= -1.0f; // Flip Y for Vulkan

	return lightProj * lightView;
}

void Renderer::updateShadowData() {
	if (!_currentRenderParams || !_currentRenderParams->lights || _currentRenderParams->lights->empty()) {
		_shadowCasterCount = 0;
		return;
	}

	// Step 1: Compute camera frustum corners (shared by all lights)
	constexpr float shadowDistance = 10.0f;
	float aspect = static_cast<float>(_swapChain->getExtent().width)
				 / static_cast<float>(_swapChain->getExtent().height);

	glm::mat4 limitedProj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, shadowDistance);
	limitedProj[1][1] *= -1.0f; // Vulkan Y-flip
	glm::mat4 invViewProj = glm::inverse(limitedProj * _currentRenderParams->viewMatrix);

	std::array<glm::vec4, 8> ndcCorners = {{
		{-1, -1, 0, 1}, {1, -1, 0, 1}, {-1, 1, 0, 1}, {1, 1, 0, 1},
		{-1, -1, 1, 1}, {1, -1, 1, 1}, {-1, 1, 1, 1}, {1, 1, 1, 1}
	}};

	std::array<glm::vec3, 8> worldCorners;
	for (int i = 0; i < 8; ++i) {
		glm::vec4 p = invViewProj * ndcCorners[i];
		worldCorners[i] = glm::vec3(p) / p.w;
	}

	// Step 2: Build ShadowData for each shadow-casting directional light
	std::vector<ShadowData> shadows;
	uint32_t shadowIdx = 0;
	constexpr float atlasSize = static_cast<float>(SHADOW_ATLAS_SIZE);
	constexpr float tileNorm = static_cast<float>(SHADOW_TILE_SIZE) / atlasSize;

	for (auto& light : *_currentRenderParams->lights) {
		if (light.positionAndType.w != 0) continue;    // Directional only
		if (light.spotAngles.z < 0) continue;           // Not a shadow caster
		if (shadowIdx >= MAX_SHADOW_TILES) break;       // Atlas full

		glm::vec3 lightDir = glm::normalize(glm::vec3(light.directionAndRange));
		auto [tileX, tileY] = shadowTilePixelOffset(shadowIdx);

		ShadowData sd;
		sd.lightSpaceMatrix = computeFrustumLightSpaceMatrix(lightDir, worldCorners);
		sd.atlasRegion = glm::vec4(
			static_cast<float>(tileX) / atlasSize,
			static_cast<float>(tileY) / atlasSize,
			tileNorm, tileNorm
		);
		shadows.push_back(sd);

		// Patch shadowIndex in-place
		light.spotAngles.z = static_cast<float>(shadowIdx);
		shadowIdx++;
	}

	_shadowCasterCount = shadowIdx;

	// Step 3: Upload to SSBO
	if (shadows.empty()) return;

	auto& frame = _frames[_currentFrameIndex];
	vk::DeviceSize requiredSize = shadows.size() * sizeof(ShadowData);

	if (!frame.shadowDataBuffer || frame.shadowDataBuffer->getSize() < requiredSize) {
		vk::DeviceSize allocSize =
			std::max(requiredSize, static_cast<vk::DeviceSize>(MAX_SHADOW_TILES * sizeof(ShadowData)));

		frame.shadowDataBuffer = std::make_unique<VulkanBuffer>(
			_device, allocSize,
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		frame.shadowDataBuffer->map();
	}
	frame.shadowDataBuffer->upload(shadows.data(), requiredSize);
}

bool Renderer::beginFrame() {
	if (_isFrameStarted) {
		throw std::runtime_error("Can't call beginFrame while already in progress");
	}

	auto result = _device->waitForFences(*_frames[_currentFrameIndex].inFlightFence, vk::True, FENCE_TIMEOUT);
	if (result != vk::Result::eSuccess) {
		throw std::runtime_error("WaitForFences failed");
	}

	vk::Result acquireResult;
	uint32_t imageIndex;
	try {
		auto [result, idx] = _swapChain->get().acquireNextImage(
			FENCE_TIMEOUT, *_frames[_currentFrameIndex].imageAvailableSemaphore, nullptr);
		acquireResult = result;
		imageIndex = idx;
	} catch (const vk::OutOfDateKHRError&) {
		recreateSwapChain();
		return false;
	}
	_currentImageIndex = imageIndex;

	if (acquireResult == vk::Result::eSuboptimalKHR) {
		// Suboptimal is still usable, continue
	} else if (acquireResult != vk::Result::eSuccess) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	_device->resetFences(*_frames[_currentFrameIndex].inFlightFence);

	_isFrameStarted = true;

	_frames[_currentFrameIndex].commandBuffer.reset();
	vk::CommandBufferBeginInfo beginInfo{};
	_frames[_currentFrameIndex].commandBuffer.begin(beginInfo);

	return true;
}

void Renderer::endFrame() {
	if (!_isFrameStarted) {
		throw std::runtime_error("Can't call endFrame if frame not started");
	}

	_frames[_currentFrameIndex].commandBuffer.end();

	std::array<vk::PipelineStageFlags, 1> waitStages = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
	vk::CommandBuffer rawCmd = *_frames[_currentFrameIndex].commandBuffer;

	vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
							  .pWaitSemaphores = &*_frames[_currentFrameIndex].imageAvailableSemaphore,
							  .pWaitDstStageMask = waitStages.data(),
							  .commandBufferCount = 1,
							  .pCommandBuffers = &rawCmd,
							  .signalSemaphoreCount = 1,
							  .pSignalSemaphores = &*_renderFinishedSemaphores[_currentImageIndex]};

	_device.getGraphicsQueue().submit(submitInfo, *_frames[_currentFrameIndex].inFlightFence);

	vk::PresentInfoKHR presentInfo{.waitSemaphoreCount = 1,
								   .pWaitSemaphores = &*_renderFinishedSemaphores[_currentImageIndex],
								   .swapchainCount = 1,
								   .pSwapchains = &*_swapChain->get(),
								   .pImageIndices = &_currentImageIndex};

	try {
		vk::Result presentResult = _device.getPresentQueue().presentKHR(presentInfo);
		if (presentResult == vk::Result::eSuboptimalKHR || _window.wasWindowResized()) {
			_window.resetWindowResizedFlag();
			recreateSwapChain();
		}
	} catch (const vk::OutOfDateKHRError&) {
		_window.resetWindowResizedFlag();
		recreateSwapChain();
	}

	_isFrameStarted = false;
	_currentFrameIndex = (_currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

float Renderer::getAspectRatio() const {
	return static_cast<float>(_swapChain->getExtent().width) / static_cast<float>(_swapChain->getExtent().height);
}

vk::Format Renderer::findDepthFormat() {
	std::vector<vk::Format> candidates = {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
										  vk::Format::eD24UnormS8Uint};
	vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
	vk::FormatFeatureFlags features = vk::FormatFeatureFlagBits::eDepthStencilAttachment;

	for (vk::Format format : candidates) {
		vk::FormatProperties props = _device.getPhysicalDevice().getFormatProperties(format);

		if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
			FISHY_LOG_TRACE("Found depth format (linear): {}", vk::to_string(format));
			return format;
		} else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
			FISHY_LOG_TRACE("Found depth format (optimal): {}", vk::to_string(format));
			return format;
		}
	}

	FISHY_LOG_ERROR("Failed to find supported depth format!");
	throw std::runtime_error("failed to find supported depth format!");
}

void Renderer::createDepthResources() {
	_depthFormat = findDepthFormat();
	vk::Extent2D extent = getSwapChainExtent();

	FISHY_LOG_TRACE("Creating depth resources: {}x{} format:{}", extent.width, extent.height,
					vk::to_string(_depthFormat));

	_depthImage = std::make_unique<RenderTexture>(
		_device, extent.width, extent.height, _depthFormat,
		vk::ImageUsageFlagBits::eDepthStencilAttachment);

	FISHY_LOG_TRACE("Depth resources created via RenderTexture");
}

void Renderer::createSwapChainImageViews() {
	auto images = _swapChain->getImages();
	_swapChainImageViews.clear();
	_swapChainImageViews.reserve(images.size());

	vk::ImageViewCreateInfo imageViewCreateInfo{.viewType = vk::ImageViewType::e2D,
												.format = _swapChain->getFormat(),
												.subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
																	 .baseMipLevel = 0,
																	 .levelCount = 1,
																	 .baseArrayLayer = 0,
																	 .layerCount = 1}};
	for (auto image : images) {
		imageViewCreateInfo.image = image;
		_swapChainImageViews.emplace_back(*_device, imageViewCreateInfo);
	}
}

void Renderer::setIBLEnvironment(IBLEnvironment* ibl) {
	_iblEnvironment = ibl;
	if (ibl) {
		writeTextureDescriptors();
		FISHY_LOG_TRACE("IBL environment set with {} mip levels", ibl->prefilteredMipLevels);
	}
}

void Renderer::writeTextureDescriptors() {
	if (!_iblEnvironment) {
		return;
	}

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		std::array<vk::DescriptorImageInfo, 4> imageInfos{};

		// Irradiance map (binding 1)
		imageInfos[0] = {
			.sampler = *_iblEnvironment->irradianceMap->getSampler(),
			.imageView = *_iblEnvironment->irradianceMap->getImageView(),
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		// Pre-filtered env map (binding 2)
		imageInfos[1] = {
			.sampler = *_iblEnvironment->prefilteredMap->getSampler(),
			.imageView = *_iblEnvironment->prefilteredMap->getImageView(),
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		// BRDF LUT (binding 2)
		imageInfos[2] = {
			.sampler = *_iblEnvironment->brdfLUT->getSampler(),
			.imageView = *_iblEnvironment->brdfLUT->getImageView(),
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		// Shadow Map (binding 3)
		auto* shadowMapPass = _renderGraph.getPass<ShadowMapPass>();
		imageInfos[3] = {
			.sampler = shadowMapPass->getShadowMapSampler(),
			.imageView = shadowMapPass->getShadowMapView(),
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		std::array<vk::WriteDescriptorSet, 4> descriptorWrites{};

		for (uint32_t j = 0; j < 4; j++) {
			descriptorWrites[j] = {
				.dstSet = *_frames[i].descriptorSet,
				.dstBinding = j, // Bindings 0, 1, 2
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.pImageInfo = &imageInfos[j],
			};
		}

		_device->updateDescriptorSets(descriptorWrites, {});
	}

	FISHY_LOG_TRACE("IBL descriptors written to global descriptor sets");
}

void Renderer::updateInstanceDataBuffer() {
	if (_instanceData.empty()) {
		return;
	}

	auto& frame = _frames[_currentFrameIndex];
	vk::DeviceSize requiredSize = _instanceData.size() * sizeof(InstanceData);

	// Recreate buffer if too small
	if (!frame.instanceDataBuffer || frame.instanceDataBuffer->getSize() < requiredSize) {
		// Round up to reasonable capacity with some headroom
		vk::DeviceSize allocSize =
			std::max(requiredSize, static_cast<vk::DeviceSize>(INITIAL_MAX_OBJECTS * sizeof(InstanceData)));

		frame.instanceDataBuffer = std::make_unique<VulkanBuffer>(
			_device, allocSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		frame.instanceDataBuffer->map();

		// BDA: No descriptor update needed - address is passed via push constants
		FISHY_LOG_TRACE("[Bindless] Frame {} allocated instance SSBO ({} bytes)", _currentFrameIndex, allocSize);

#ifndef NDEBUG
		std::string debugName = "InstanceSSBO_Frame" + std::to_string(_currentFrameIndex);
		VulkanUtils::setDebugName(_device, frame.instanceDataBuffer->getBuffer(), debugName.c_str());
#endif
	}

	// Upload data
	frame.instanceDataBuffer->upload(_instanceData.data(), requiredSize);
}

void Renderer::buildUnifiedBuffersFromScene(Scene& scene) {
	std::vector<Vertex> allVertices;
	std::vector<uint32_t> allIndices;
	_meshRegions.clear();

	auto view = scene.view<MeshComponent, MeshRendererComponent, TransformComponent>();
	for (auto entity : view) {
		auto& mesh = view.get<MeshComponent>(entity);
		const auto& meshRenderer = view.get<MeshRendererComponent>(entity);

		if (!mesh.mesh || !meshRenderer.visible) {
			_meshRegions.push_back({0, 0, 0});
			continue;
		}

		const auto& vertices = mesh.mesh->getVertices();
		const auto& indices = mesh.mesh->getIndices();

		MeshRegion region{
			.firstIndex = static_cast<uint32_t>(allIndices.size()),
			.indexCount = static_cast<uint32_t>(indices.size()),
			.vertexOffset = static_cast<int32_t>(allVertices.size()),
		};
		mesh.meshRegionIndex = static_cast<uint32_t>(_meshRegions.size());
		_meshRegions.push_back(region);

		allVertices.insert(allVertices.end(), vertices.begin(), vertices.end());
		allIndices.insert(allIndices.end(), indices.begin(), indices.end());
	}

	if (allVertices.empty() || allIndices.empty()) {
		return;
	}

	vk::DeviceSize vertexBufferSize = allVertices.size() * sizeof(Vertex);
	_unifiedVertexBuffer = std::make_unique<VulkanBuffer>(
		_device, vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::DeviceSize indexBufferSize = allIndices.size() * sizeof(uint32_t);
	_unifiedIndexBuffer = std::make_unique<VulkanBuffer>(
		_device, indexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

#ifndef NDEBUG
	VulkanUtils::setDebugName(_device, _unifiedVertexBuffer->getBuffer(), "UnifiedVertexBuffer");
	VulkanUtils::setDebugName(_device, _unifiedIndexBuffer->getBuffer(), "UnifiedIndexBuffer");
#endif

	auto stagingVertex =
		VulkanBuffer(_device, vertexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
					 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	stagingVertex.map();
	stagingVertex.upload(allVertices.data(), vertexBufferSize);

	auto stagingIndex =
		VulkanBuffer(_device, indexBufferSize, vk::BufferUsageFlagBits::eTransferSrc,
					 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	stagingIndex.map();
	stagingIndex.upload(allIndices.data(), indexBufferSize);

	VulkanUtils::executeImmediate(_device, [&](auto& cmd) {
		cmd.copyBuffer(stagingVertex.getBuffer(), _unifiedVertexBuffer->getBuffer(),
					   vk::BufferCopy{.size = vertexBufferSize});
		cmd.copyBuffer(stagingIndex.getBuffer(), _unifiedIndexBuffer->getBuffer(),
					   vk::BufferCopy{.size = indexBufferSize});
	});

	_unifiedBuffersDirty = false;
	FISHY_LOG_TRACE("Built unified buffers from scene: {} vertices, {} indices", allVertices.size(), allIndices.size());
}

void Renderer::buildInstanceData(Scene& scene) {
	_instanceData.clear();

	if (!_currentRenderParams || !_currentRenderParams->visibleEntities) {
		return;
	}

	auto& registry = scene.getRegistry();
	for (auto entity : *_currentRenderParams->visibleEntities) {
		const auto& meshRenderer = registry.get<MeshRendererComponent>(entity);
		const auto& transform = registry.get<TransformComponent>(entity);

		// Build instance data from transform
		InstanceData inst{};
		inst.model = transform.worldMatrix;

		// Fill material properties using entt::resource operator->
		if (meshRenderer.material) {
			const Material& mat = *meshRenderer.material;
			inst.baseColorFactor = mat.params.baseColorFactor;
			inst.emissiveFactor = glm::vec4(mat.params.emissiveFactor, mat.params.emissiveStrength);

			// Packed PBR factors: x=metallic, y=roughness, z=normalScale, w=occlusionStrength
			inst.pbrFactors = glm::vec4(mat.params.metallicFactor, mat.params.roughnessFactor, mat.params.normalScale,
										mat.params.occlusionStrength);

			// Advanced factors: x=alphaCutoff, y=transmission, z=ior, w=clearcoatFactor
			inst.extraFactors1 = glm::vec4(mat.params.alphaCutoff, mat.params.transmissionFactor, mat.params.ior,
										   mat.params.clearcoatFactor);

			// Clearcoat details: x=clearcoatRoughness, yzw=unused
			inst.extraFactors2 = glm::vec4(mat.params.clearcoatRoughnessFactor, 0.0f, 0.0f, 0.0f);

			// Get texture indices from pre-computed material indices (avoids per-frame hash lookups)
			inst.baseColorIndex = mat.textureIndices.baseColor;
			inst.metallicRoughnessIndex = mat.textureIndices.metallicRoughness;
			inst.normalIndex = mat.textureIndices.normal;
			inst.occlusionIndex = mat.textureIndices.occlusion;
			inst.emissiveIndex = mat.textureIndices.emissive;

			// Extension texture indices
			inst.clearcoatIndex = mat.textureIndices.clearcoat;
			inst.clearcoatRoughnessIndex = mat.textureIndices.clearcoatRoughness;
			inst.clearcoatNormalIndex = mat.textureIndices.clearcoatNormal;
			inst.transmissionIndex = mat.textureIndices.transmission;

			// Build texture flags - MUST match shader expectations in PBRshader.slang
			// Shader reads: alphaMode = (flags >> 1) & 0x3, hasNormalMap = flags & (1<<5), hasOcclusion = flags &
			// (1<<6)
			uint32_t flags = 0;
			// AlphaMode uses bits 1-2 (2 bits: 0=OPAQUE, 1=MASK, 2=BLEND)
			flags |= (static_cast<uint32_t>(mat.params.alphaMode) << 1);
			// Texture presence flags
			if (mat.normalMap)
				flags |= (1 << 5); // bit 5 = HasNormalMap
			if (mat.occlusionMap)
				flags |= (1 << 6); // bit 6 = HasOcclusion
			if (mat.params.doubleSided)
				flags |= (1 << 9);
			inst.flags = flags;
		}

		_instanceData.push_back(inst);
	}
}

void Renderer::buildDrawBatches(Scene& scene) {
	if (_unifiedBuffersDirty) {
		buildUnifiedBuffersFromScene(scene);
	}

	_drawBatches.clear();
	_indirectCommands.clear();

	if (_meshRegions.empty() || !_currentRenderParams || !_currentRenderParams->visibleEntities) {
		return;
	}

	_drawBatches.push_back({
		.firstCommand = 0,
		.commandCount = 0,
	});

	auto& registry = scene.getRegistry();
	for (auto entity : *_currentRenderParams->visibleEntities) {
		const auto& meshComp = registry.get<MeshComponent>(entity);

		if (meshComp.meshRegionIndex == UINT32_MAX || _meshRegions[meshComp.meshRegionIndex].indexCount == 0) {
			continue;
		}

		const auto& region = _meshRegions[meshComp.meshRegionIndex];

		vk::DrawIndexedIndirectCommand cmd{
			.indexCount = region.indexCount,
			.instanceCount = 1,
			.firstIndex = region.firstIndex,
			.vertexOffset = region.vertexOffset,
			.firstInstance = static_cast<uint32_t>(_indirectCommands.size()),
		};
		_indirectCommands.push_back(cmd);
		_drawBatches.back().commandCount++;
	}

	if (!_indirectCommands.empty()) {
		_frames[_currentFrameIndex].indirectBuffer->upload(
			_indirectCommands.data(), _indirectCommands.size() * sizeof(vk::DrawIndexedIndirectCommand));
	}
}

} // namespace Fishy
