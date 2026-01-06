#include "Renderer.h"

#include "../ecs/components/MeshComponent.h"
#include "../ecs/components/MeshRendererComponent.h"
#include "../ecs/components/TransformComponent.h"
#include "../ecs/systems/RenderSystem.h" // For RenderParams
#include "../scene/Scene.h"
#include "PipelineBuilder.h"
#include "core/CommandPool.h"
#include "core/LogSystem.h"
#include "core/VulkanDevice.h"
#include "core/VulkanUtils.h"
#include "core/Window.h"
#include "resources/ResourceManager.h"
#include "vulkan/vulkan.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace Fishy {

static constexpr uint64_t FENCE_TIMEOUT = std::numeric_limits<uint32_t>::max();

Renderer::Renderer(VulkanDevice& device, Window& window, ResourceManager& resourceManager)
	: _device(device), _window(window), _resourceManager(resourceManager) {
	LogSystem::get().info("Initializing Renderer...");
	_frames.resize(MAX_FRAMES_IN_FLIGHT);
	recreateSwapChain();
	createCommandBuffers();

	// Descriptor set layouts
	createGlobalSetLayout();
	createBindlessTextureSetLayout(); // Creates both texture and SSBO bindless layouts

	// Descriptor pools
	createDescriptorPool();			// For global sets
	createBindlessDescriptorPool(); // For bindless sets (UPDATE_AFTER_BIND)

	loadPipelineCache();
	createGraphicsPipeline();

	createUniformBuffers();
	createIndirectBuffer();

	// Allocate all descriptor sets (global + bindless) and write initial data
	allocateDescriptorSets();

	// Initialize ResourceManager with bindless sets (enables auto-registration)
	_resourceManager.initBindlessResources(*_bindlessTextureSet, *_bindlessStorageBufferSet);

	createSyncObjects();

	LogSystem::get().info("Renderer initialized successfully");
}

Renderer::~Renderer() {
	LogSystem::get().info("Shutting down Renderer...");
	savePipelineCache();
	_device->waitIdle();

	freeCommandBuffers();
	_swapChain.reset();

	_unifiedVertexBuffer.reset();
	_unifiedIndexBuffer.reset();

	// Destroy depth image with VMA
	if (_depthAllocation) {
		vmaDestroyImage(_device.getVmaAllocator(), _depthImage, _depthAllocation);
	}
}

void Renderer::renderScene(Scene& scene, const RenderParams& params,
						   std::function<void(VkCommandBuffer)> uiRenderCallback) {
	if (!beginFrame()) {
		return;
	}

	const auto& cmd = _frames[_currentFrameIndex].commandBuffer;

	// Store params for updateUniformBuffer
	_currentRenderParams = &params;
	updateUniformBuffer(_currentFrameIndex);

	VulkanUtils::transitionImage(*cmd, _swapChain->getImages()[_currentImageIndex], vk::ImageLayout::eUndefined,
								 vk::ImageLayout::eColorAttachmentOptimal, {},
								 vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eTopOfPipe,
								 vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor);

	vk::ClearValue clearColor{.color = {.float32 = {{0.01f, 0.01f, 0.02f, 1.0f}}}};
	vk::ClearValue clearDepth{.depthStencil = {1.0f, 0}};
	vk::Extent2D extent = _swapChain->getExtent();

	vk::RenderingAttachmentInfo colorAttachment{.imageView = *_swapChainImageViews[_currentImageIndex],
												.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
												.loadOp = vk::AttachmentLoadOp::eClear,
												.storeOp = vk::AttachmentStoreOp::eStore,
												.clearValue = clearColor};

	vk::RenderingAttachmentInfo depthAttachment{.imageView = *_depthImageView,
												.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
												.loadOp = vk::AttachmentLoadOp::eClear,
												.storeOp = vk::AttachmentStoreOp::eDontCare,
												.clearValue = clearDepth};

	vk::RenderingInfo renderingInfo{.renderArea = vk::Rect2D{{0, 0}, extent},
									.layerCount = 1,
									.colorAttachmentCount = 1,
									.pColorAttachments = &colorAttachment,
									.pDepthAttachment = &depthAttachment};

	cmd.beginRendering(renderingInfo);

	_graphicsPipeline->bind(cmd);

	cmd.setViewport(
		0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f));
	cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_graphicsPipeline->getLayout(), 0,
						   *_frames[_currentFrameIndex].descriptorSet, nullptr);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_graphicsPipeline->getLayout(), 1, *_bindlessTextureSet,
						   nullptr);
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_graphicsPipeline->getLayout(), 2,
						   *_bindlessStorageBufferSet, nullptr);

	// Build data from scene entities
	buildInstanceDataFromScene(scene);
	updateInstanceDataBuffer();
	buildDrawBatchesFromScene(scene);

	renderIndirect(cmd);

	cmd.endRendering();

	if (uiRenderCallback) {
		vk::RenderingAttachmentInfo uiColorAttachment{.imageView = *_swapChainImageViews[_currentImageIndex],
													  .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
													  .loadOp = vk::AttachmentLoadOp::eLoad,
													  .storeOp = vk::AttachmentStoreOp::eStore};

		vk::RenderingInfo uiRenderingInfo{.renderArea = vk::Rect2D{{0, 0}, extent},
										  .layerCount = 1,
										  .colorAttachmentCount = 1,
										  .pColorAttachments = &uiColorAttachment};

		cmd.beginRendering(uiRenderingInfo);
		uiRenderCallback(static_cast<VkCommandBuffer>(*cmd));
		cmd.endRendering();
	}

	VulkanUtils::transitionImage(*cmd, _swapChain->getImages()[_currentImageIndex],
								 vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
								 vk::AccessFlagBits2::eColorAttachmentWrite, {},
								 vk::PipelineStageFlagBits2::eColorAttachmentOutput,
								 vk::PipelineStageFlagBits2::eBottomOfPipe, vk::ImageAspectFlagBits::eColor);

	endFrame();
}

void Renderer::recreateSwapChain() {
	auto extent = _window.getExtent();
	while (extent.width == 0 || extent.height == 0) {
		extent = _window.getExtent();
		glfwWaitEvents();
	}

	LogSystem::get().info("Recreating swap chain: {}x{}", extent.width, extent.height);
	_device->waitIdle();

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

void Renderer::createGlobalSetLayout() {
	// Set 0: Global data + IBL textures
	// Binding 0: GlobalUBO
	// Binding 1: irradianceMap (samplerCube)
	// Binding 2: prefilteredEnvMap (samplerCube)
	// Binding 3: brdfLUT (sampler2D)
	std::vector<vk::DescriptorSetLayoutBinding> bindings = {
		{.binding = 0,
		 .descriptorType = vk::DescriptorType::eUniformBuffer,
		 .descriptorCount = 1,
		 .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment},
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

	_globalSetLayout = vk::raii::DescriptorSetLayout(*_device, layoutInfo);
}

void Renderer::createBindlessTextureSetLayout() {
	LogSystem::get().info("[Bindless] Creating texture and SSBO set layouts (max {} entries)", MAX_BINDLESS_TEXTURES);

	// ═══════════════════════════════════════════════════════════════════════════
	// Set 1: Bindless Texture Array
	// ═══════════════════════════════════════════════════════════════════════════
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

	vk::DescriptorSetLayoutCreateInfo textureLayoutInfo{
		.pNext = &textureBindingFlagsInfo,
		.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool, // Required for UPDATE_AFTER_BIND
		.bindingCount = 1,
		.pBindings = &textureBinding};

	_bindlessTextureSetLayout = vk::raii::DescriptorSetLayout(*_device, textureLayoutInfo);
	LogSystem::get().info("[Bindless] Texture set layout created");

	// ═══════════════════════════════════════════════════════════════════════════
	// Set 2: Bindless Storage Buffer Array
	// ═══════════════════════════════════════════════════════════════════════════
	vk::DescriptorBindingFlags ssboBindingFlags = vk::DescriptorBindingFlagBits::eUpdateAfterBind |
												  vk::DescriptorBindingFlagBits::ePartiallyBound |
												  vk::DescriptorBindingFlagBits::eVariableDescriptorCount;

	vk::DescriptorSetLayoutBindingFlagsCreateInfo ssboBindingFlagsInfo{.bindingCount = 1,
																	   .pBindingFlags = &ssboBindingFlags};

	vk::DescriptorSetLayoutBinding ssboBinding{.binding = 0,
											   .descriptorType = vk::DescriptorType::eStorageBuffer,
											   .descriptorCount = MAX_BINDLESS_TEXTURES, // Same capacity
											   .stageFlags = vk::ShaderStageFlagBits::eVertex |
															 vk::ShaderStageFlagBits::eFragment};

	vk::DescriptorSetLayoutCreateInfo ssboLayoutInfo{.pNext = &ssboBindingFlagsInfo,
													 .flags =
														 vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
													 .bindingCount = 1,
													 .pBindings = &ssboBinding};

	_bindlessStorageBufferSetLayout = vk::raii::DescriptorSetLayout(*_device, ssboLayoutInfo);
	LogSystem::get().info("[Bindless] Storage buffer set layout created");
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
	LogSystem::get().info("[Bindless] Descriptor pool created with UPDATE_AFTER_BIND flag");
}

void Renderer::allocateDescriptorSets() {
	// ═══════════════════════════════════════════════════════════════════════════
	// Allocate Bindless Texture Set (Set 1)
	// ═══════════════════════════════════════════════════════════════════════════
	uint32_t textureVariableCount = MAX_BINDLESS_TEXTURES;

	vk::DescriptorSetVariableDescriptorCountAllocateInfo textureVariableInfo{
		.descriptorSetCount = 1, .pDescriptorCounts = &textureVariableCount};

	vk::DescriptorSetAllocateInfo textureAllocInfo{.pNext = &textureVariableInfo,
												   .descriptorPool = *_bindlessDescriptorPool,
												   .descriptorSetCount = 1,
												   .pSetLayouts = &*_bindlessTextureSetLayout};

	auto textureSets = vk::raii::DescriptorSets(*_device, textureAllocInfo);
	_bindlessTextureSet = std::move(textureSets[0]);
	LogSystem::get().info("[Bindless] Texture descriptor set allocated");

	// ═══════════════════════════════════════════════════════════════════════════
	// Allocate Bindless SSBO Set (Set 2)
	// ═══════════════════════════════════════════════════════════════════════════
	uint32_t ssboVariableCount = MAX_BINDLESS_TEXTURES;

	vk::DescriptorSetVariableDescriptorCountAllocateInfo ssboVariableInfo{.descriptorSetCount = 1,
																		  .pDescriptorCounts = &ssboVariableCount};

	vk::DescriptorSetAllocateInfo ssboAllocInfo{.pNext = &ssboVariableInfo,
												.descriptorPool = *_bindlessDescriptorPool,
												.descriptorSetCount = 1,
												.pSetLayouts = &*_bindlessStorageBufferSetLayout};

	auto ssboSets = vk::raii::DescriptorSets(*_device, ssboAllocInfo);
	_bindlessStorageBufferSet = std::move(ssboSets[0]);
	LogSystem::get().info("[Bindless] Storage buffer descriptor set allocated");

	// ═══════════════════════════════════════════════════════════════════════════
	// Allocate Global Descriptor Sets (Set 0: UBO + IBL textures)
	// ═══════════════════════════════════════════════════════════════════════════
	std::vector<vk::DescriptorSetLayout> globalLayouts(MAX_FRAMES_IN_FLIGHT, *_globalSetLayout);

	vk::DescriptorSetAllocateInfo globalAllocInfo{.descriptorPool = *_descriptorPool,
												  .descriptorSetCount = static_cast<uint32_t>(globalLayouts.size()),
												  .pSetLayouts = globalLayouts.data()};

	auto globalSets = vk::raii::DescriptorSets(*_device, globalAllocInfo);

	// Get default textures for placeholders
	auto defaultTex = _resourceManager.getDefaultWhiteTexture();
	auto defaultCubemap = _resourceManager.getDefaultCubemap();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_frames[i].descriptorSet = std::move(globalSets[i]);

		// UBO descriptor
		vk::DescriptorBufferInfo uboInfo{
			.buffer = _frames[i].uniformBuffer->getBuffer(), .offset = 0, .range = sizeof(UniformBufferObject)};

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

		std::array<vk::WriteDescriptorSet, 4> descriptorWrites{};

		// Binding 0: Global UBO
		descriptorWrites[0].dstSet = *_frames[i].descriptorSet;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &uboInfo;

		// Binding 1: IBL irradiance map
		descriptorWrites[1].dstSet = *_frames[i].descriptorSet;
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &cubemapImageInfo;

		// Binding 2: IBL prefiltered map
		descriptorWrites[2].dstSet = *_frames[i].descriptorSet;
		descriptorWrites[2].dstBinding = 2;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].pImageInfo = &cubemapImageInfo;

		// Binding 3: BRDF LUT
		descriptorWrites[3].dstSet = *_frames[i].descriptorSet;
		descriptorWrites[3].dstBinding = 3;
		descriptorWrites[3].dstArrayElement = 0;
		descriptorWrites[3].descriptorType = vk::DescriptorType::eCombinedImageSampler;
		descriptorWrites[3].descriptorCount = 1;
		descriptorWrites[3].pImageInfo = &brdfLutImageInfo;

		_device->updateDescriptorSets(descriptorWrites, {});
	}
	LogSystem::get().info("All descriptor sets allocated and initialized");
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
	}
	LogSystem::get().trace("Created indirect draw buffers ({} bytes each)", bufferSize);
}

void Renderer::renderIndirect(const vk::raii::CommandBuffer& cmd) {
	if (!_unifiedVertexBuffer || !_unifiedIndexBuffer || _drawBatches.empty()) {
		return;
	}

	// Bind unified vertex and index buffers ONCE for all draw calls
	cmd.bindVertexBuffers(0, _unifiedVertexBuffer->getBuffer(), {0});
	cmd.bindIndexBuffer(_unifiedIndexBuffer->getBuffer(), 0, vk::IndexType::eUint32);

	// Render using indirect draw calls
	// With bindless, we don't need to rebind per-material - all textures accessible via indices
	for (const auto& batch : _drawBatches) {
		// Issue indirect draw call for all commands in this batch
		cmd.drawIndexedIndirect(_frames[_currentFrameIndex].indirectBuffer->getBuffer(),
								batch.firstCommand * sizeof(vk::DrawIndexedIndirectCommand), batch.commandCount,
								sizeof(vk::DrawIndexedIndirectCommand));
	}
}

void Renderer::createGraphicsPipeline() {
	LogSystem::get().info("Creating graphics pipeline...");
	const auto& shaderModule = _resourceManager.getShader("shaders/PBRshader.slang.spv");

	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.data()};

	PipelineBuilder builder(**_device);

	builder.setShaders(shaderModule, shaderModule, "vertMain", "fragMain")
		.setVertexInput(vertexInputInfo)
		.setInputTopology(vk::PrimitiveTopology::eTriangleList)
		.setCullMode(vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise)
		.setLayout({*_globalSetLayout, *_bindlessTextureSetLayout, *_bindlessStorageBufferSetLayout}, {})
		.setRenderingFormats({_swapChain->getFormat()}, _depthFormat)
		.setDepthStencilTest(true, true, vk::CompareOp::eLess, false, vk::CompareOp::eAlways);

	_graphicsPipeline = builder.build(*_device, nullptr, _pipelineCache);
	LogSystem::get().info("Graphics pipeline created successfully");
}

void Renderer::createUniformBuffers() {
	vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_frames[i].uniformBuffer = std::make_unique<VulkanBuffer>(
			_device, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		// Persistent mapping: map once and keep it mapped
		_frames[i].uniformBuffer->map();
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

void Renderer::loadPipelineCache() {
	std::vector<char> cacheData;
	std::ifstream file(PIPELINE_CACHE_FILENAME, std::ios::binary | std::ios::ate);
	if (file.is_open()) {
		size_t fileSize = static_cast<size_t>(file.tellg());
		cacheData.resize(fileSize);
		file.seekg(0);
		file.read(cacheData.data(), fileSize);
		LogSystem::get().info("Loaded pipeline cache: {} bytes", fileSize);
	} else {
		LogSystem::get().info("No existing pipeline cache found, creating new one");
	}

	vk::PipelineCacheCreateInfo cacheInfo{.initialDataSize = cacheData.size(),
										  .pInitialData = cacheData.empty() ? nullptr : cacheData.data()};
	_pipelineCache = vk::raii::PipelineCache(*_device, cacheInfo);
}

void Renderer::savePipelineCache() {
	if (!*_pipelineCache) {
		return;
	}
	auto cacheData = _pipelineCache.getData();
	std::ofstream file(PIPELINE_CACHE_FILENAME, std::ios::binary);
	if (file.is_open()) {
		file.write(reinterpret_cast<const char*>(cacheData.data()), cacheData.size());
		LogSystem::get().info("Saved pipeline cache: {} bytes", cacheData.size());
	} else {
		LogSystem::get().warn("Failed to save pipeline cache to disk");
	}
}

void Renderer::updateUniformBuffer(uint32_t frameIndex) {
	auto extent = _swapChain->getExtent();
	UniformBufferObject ubo{};

	// Use RenderParams from ECS systems
	if (_currentRenderParams) {
		ubo.view = _currentRenderParams->viewMatrix;
		ubo.proj = _currentRenderParams->projectionMatrix;
		ubo.camPos = _currentRenderParams->cameraPosition;
		ubo.lightDir = glm::normalize(_currentRenderParams->lightDirection);
		ubo.lightColor = _currentRenderParams->lightColor;
	} else {
		// Fallback defaults if no RenderParams (shouldn't happen in normal use)
		ubo.view = glm::mat4(1.0f);
		ubo.proj = glm::perspective(glm::radians(45.0f),
									static_cast<float>(extent.width) / static_cast<float>(extent.height), 0.1f, 100.0f);
		ubo.camPos = glm::vec3(0.0f, 0.0f, 3.0f);
		ubo.lightDir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
		ubo.lightColor = glm::vec3(5.0f, 5.0f, 5.0f);
	}

	// Debug settings
	ubo.debugViewInputs = _debugViewInputs;
	ubo.debugViewEquation = _debugViewEquation;

	// IBL parameters
	if (_iblEnvironment) {
		ubo.prefilteredMipLevels = static_cast<float>(_iblEnvironment->prefilteredMipLevels);
		ubo.iblIntensity = 1.0f;
	} else {
		ubo.prefilteredMipLevels = 1.0f;
		ubo.iblIntensity = 0.0f; // Disable IBL if no environment
	}

	_frames[frameIndex].uniformBuffer->upload(&ubo, sizeof(ubo));
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
			LogSystem::get().trace("Found depth format (linear): {}", vk::to_string(format));
			return format;
		} else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
			LogSystem::get().trace("Found depth format (optimal): {}", vk::to_string(format));
			return format;
		}
	}

	LogSystem::get().error("Failed to find supported depth format!");
	throw std::runtime_error("failed to find supported depth format!");
}

void Renderer::createDepthResources() {
	// Destroy old depth resources if they exist (critical for recreateSwapChain)
	// This prevents VMA "Unfreed dedicated allocations" error on program exit
	if (_depthAllocation) {
		// Reset ImageView first (it references the image)
		_depthImageView = nullptr;
		vmaDestroyImage(_device.getVmaAllocator(), _depthImage, _depthAllocation);
		_depthImage = VK_NULL_HANDLE;
		_depthAllocation = nullptr;
	}

	_depthFormat = findDepthFormat();
	vk::Extent2D extent = getSwapChainExtent();

	LogSystem::get().info("Creating depth resources: {}x{} format:{}", extent.width, extent.height,
						  vk::to_string(_depthFormat));

	// Create Image using VMA (keep vk:: style, convert to Vk for VMA)
	vk::ImageCreateInfo imageInfo{.imageType = vk::ImageType::e2D,
								  .format = _depthFormat,
								  .extent = {extent.width, extent.height, 1},
								  .mipLevels = 1,
								  .arrayLayers = 1,
								  .samples = vk::SampleCountFlagBits::e1,
								  .tiling = vk::ImageTiling::eOptimal,
								  .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
								  .sharingMode = vk::SharingMode::eExclusive,
								  .initialLayout = vk::ImageLayout::eUndefined};

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkResult result = vmaCreateImage(_device.getVmaAllocator(), reinterpret_cast<const VkImageCreateInfo*>(&imageInfo),
									 &allocInfo, &_depthImage, &_depthAllocation, nullptr);

	if (result != VK_SUCCESS) {
		LogSystem::get().error("Failed to create VMA depth image: {}x{}", extent.width, extent.height);
		throw std::runtime_error("Failed to create VMA depth image!");
	}

	vk::ImageViewCreateInfo viewInfo{.image = _depthImage,
									 .viewType = vk::ImageViewType::e2D,
									 .format = _depthFormat,
									 .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth,
														  .baseMipLevel = 0,
														  .levelCount = 1,
														  .baseArrayLayer = 0,
														  .layerCount = 1}};
	// Stencil aspect?
	if (_depthFormat == vk::Format::eD32SfloatS8Uint || _depthFormat == vk::Format::eD24UnormS8Uint) {
		viewInfo.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
	}

	_depthImageView = vk::raii::ImageView(*_device, viewInfo);
	LogSystem::get().trace("Created depth image view");

	// Perform explicit layout transition Undefined -> DepthStencilAttachmentOptimal
	{
		LogSystem::get().trace("Performing layout transition for depth image");
		auto aspectMask = viewInfo.subresourceRange.aspectMask;

		VulkanUtils::executeImmediate(_device, [&](auto& cmd) {
			VulkanUtils::transitionImage(
				*cmd, _depthImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal,
				vk::AccessFlagBits2::eNone,
				vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				vk::PipelineStageFlagBits2::eTopOfPipe,
				vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
				aspectMask);
		});

		LogSystem::get().trace("Layout transition for depth image completed");
	}
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
		writeIBLDescriptors();
		LogSystem::get().info("IBL environment set with {} mip levels", ibl->prefilteredMipLevels);
	}
}

void Renderer::writeIBLDescriptors() {
	if (!_iblEnvironment) {
		return;
	}

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		std::array<vk::DescriptorImageInfo, 3> imageInfos{};

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

		// BRDF LUT (binding 3)
		imageInfos[2] = {
			.sampler = *_iblEnvironment->brdfLUT->getSampler(),
			.imageView = *_iblEnvironment->brdfLUT->getImageView(),
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		std::array<vk::WriteDescriptorSet, 3> descriptorWrites{};

		for (uint32_t j = 0; j < 3; j++) {
			descriptorWrites[j] = {
				.dstSet = *_frames[i].descriptorSet,
				.dstBinding = j + 1, // Bindings 1, 2, 3
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.pImageInfo = &imageInfos[j],
			};
		}

		_device->updateDescriptorSets(descriptorWrites, {});
	}

	LogSystem::get().trace("IBL descriptors written to global descriptor sets");
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
			_device, allocSize, vk::BufferUsageFlagBits::eStorageBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		frame.instanceDataBuffer->map();

		// Register to bindless SSBO - writes to Set 2, binding 0
		vk::DescriptorBufferInfo bufferInfo{
			.buffer = frame.instanceDataBuffer->getBuffer(), .offset = 0, .range = allocSize};

		vk::WriteDescriptorSet write{.dstSet = *_bindlessStorageBufferSet,
									 .dstBinding = 0,
									 .dstArrayElement = 0, // Single SSBO at index 0
									 .descriptorCount = 1,
									 .descriptorType = vk::DescriptorType::eStorageBuffer,
									 .pBufferInfo = &bufferInfo};

		_device->updateDescriptorSets(write, {});
		LogSystem::get().trace("[Bindless] Frame {} allocated instance SSBO ({} bytes)", _currentFrameIndex, allocSize);
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
		const auto& mesh = view.get<MeshComponent>(entity);
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
	LogSystem::get().info("Built unified buffers from scene: {} vertices, {} indices", allVertices.size(),
						  allIndices.size());
}

void Renderer::buildInstanceDataFromScene(Scene& scene) {
	_instanceData.clear();

	auto view = scene.view<MeshComponent, MeshRendererComponent, TransformComponent>();
	for (auto entity : view) {
		const auto& mesh = view.get<MeshComponent>(entity);
		const auto& meshRenderer = view.get<MeshRendererComponent>(entity);
		const auto& transform = view.get<TransformComponent>(entity);

		if (!mesh.mesh || !meshRenderer.visible) {
			continue;
		}

		// Build instance data from transform
		InstanceData inst = InstanceData::fromModelMatrix(transform.worldMatrix);

		// Fill material properties using entt::resource operator->
		if (meshRenderer.material) {
			const Material& mat = *meshRenderer.material;
			inst.baseColorFactor = mat.params.baseColorFactor;
			inst.metallicFactor = mat.params.metallicFactor;
			inst.roughnessFactor = mat.params.roughnessFactor;
			inst.normalScale = mat.params.normalScale;
			inst.occlusionStrength = mat.params.occlusionStrength;
			inst.emissiveFactor = glm::vec4(mat.params.emissiveFactor, mat.params.emissiveStrength);
			inst.alphaCutoff = mat.params.alphaCutoff;

			// Extension material properties
			inst.clearcoatFactor = mat.params.clearcoatFactor;
			inst.clearcoatRoughnessFactor = mat.params.clearcoatRoughnessFactor;
			inst.transmissionFactor = mat.params.transmissionFactor;
			inst.ior = mat.params.ior;

			// Get texture indices from ResourceManager using pointer lookup
			// entt::resource uses operator* to dereference, &(*handle) gets pointer
			inst.baseColorIndex = mat.baseColorMap ? _resourceManager.getTextureBindlessIndex(&(*mat.baseColorMap))
												   : INVALID_TEXTURE_INDEX;
			inst.metallicRoughnessIndex = mat.metallicRoughnessMap
											  ? _resourceManager.getTextureBindlessIndex(&(*mat.metallicRoughnessMap))
											  : INVALID_TEXTURE_INDEX;
			inst.normalIndex =
				mat.normalMap ? _resourceManager.getTextureBindlessIndex(&(*mat.normalMap)) : INVALID_TEXTURE_INDEX;
			inst.occlusionIndex = mat.occlusionMap ? _resourceManager.getTextureBindlessIndex(&(*mat.occlusionMap))
												   : INVALID_TEXTURE_INDEX;
			inst.emissiveIndex =
				mat.emissiveMap ? _resourceManager.getTextureBindlessIndex(&(*mat.emissiveMap)) : INVALID_TEXTURE_INDEX;

			// Extension texture indices
			inst.clearcoatIndex = mat.clearcoatMap ? _resourceManager.getTextureBindlessIndex(&(*mat.clearcoatMap))
												   : INVALID_TEXTURE_INDEX;
			inst.clearcoatRoughnessIndex = mat.clearcoatRoughnessMap
											   ? _resourceManager.getTextureBindlessIndex(&(*mat.clearcoatRoughnessMap))
											   : INVALID_TEXTURE_INDEX;
			inst.clearcoatNormalIndex = mat.clearcoatNormalMap
											? _resourceManager.getTextureBindlessIndex(&(*mat.clearcoatNormalMap))
											: INVALID_TEXTURE_INDEX;
			inst.transmissionIndex = mat.transmissionMap
										 ? _resourceManager.getTextureBindlessIndex(&(*mat.transmissionMap))
										 : INVALID_TEXTURE_INDEX;

			// Build texture flags
			uint32_t flags = 0;
			if (mat.baseColorMap)
				flags |= (1 << 0);
			if (mat.metallicRoughnessMap)
				flags |= (1 << 1);
			if (mat.normalMap)
				flags |= (1 << 2);
			if (mat.occlusionMap)
				flags |= (1 << 3);
			if (mat.emissiveMap)
				flags |= (1 << 4);
			// Extension texture flags
			if (mat.clearcoatMap)
				flags |= (1 << 5);
			if (mat.clearcoatRoughnessMap)
				flags |= (1 << 6);
			if (mat.clearcoatNormalMap)
				flags |= (1 << 7);
			if (mat.transmissionMap)
				flags |= (1 << 8);
			if (mat.params.doubleSided)
				flags |= (1 << 9);
			// AlphaMode uses bits 10-11 (2 bits: 0=OPAQUE, 1=MASK, 2=BLEND)
			flags |= (static_cast<uint32_t>(mat.params.alphaMode) << 10);
			inst.flags = flags;
		}

		_instanceData.push_back(inst);
	}
}

void Renderer::buildDrawBatchesFromScene(Scene& scene) {
	if (_unifiedBuffersDirty) {
		buildUnifiedBuffersFromScene(scene);
	}

	_drawBatches.clear();
	_indirectCommands.clear();

	if (_meshRegions.empty()) {
		return;
	}

	_drawBatches.push_back({
		.material = nullptr,
		.firstCommand = 0,
		.commandCount = 0,
	});

	uint32_t i = 0;
	auto view = scene.view<MeshComponent, MeshRendererComponent, TransformComponent>();
	for (auto entity : view) {
		const auto& mesh = view.get<MeshComponent>(entity);
		const auto& meshRenderer = view.get<MeshRendererComponent>(entity);

		if (!mesh.mesh || !meshRenderer.visible || _meshRegions[i].indexCount == 0) {
			++i;
			continue;
		}

		const auto& region = _meshRegions[i];

		vk::DrawIndexedIndirectCommand cmd{
			.indexCount = region.indexCount,
			.instanceCount = 1,
			.firstIndex = region.firstIndex,
			.vertexOffset = region.vertexOffset,
			.firstInstance = static_cast<uint32_t>(_indirectCommands.size()),
		};
		_indirectCommands.push_back(cmd);
		_drawBatches.back().commandCount++;
		++i;
	}

	if (!_indirectCommands.empty()) {
		_frames[_currentFrameIndex].indirectBuffer->upload(
			_indirectCommands.data(), _indirectCommands.size() * sizeof(vk::DrawIndexedIndirectCommand));
	}
}

} // namespace Fishy