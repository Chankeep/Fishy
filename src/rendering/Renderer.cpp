#include "Renderer.h"

#include "Camera.h"
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
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace Fishy {

static constexpr uint64_t FENCE_TIMEOUT = std::numeric_limits<uint64_t>::max();

Renderer::Renderer(VulkanDevice& device, Window& window, ResourceManager& resourceManager)
	: _device(device), _window(window), _resourceManager(resourceManager) {
	LogSystem::get().info("Initializing Renderer...");
	_frames.resize(MAX_FRAMES_IN_FLIGHT);
	recreateSwapChain();
	createCommandBuffers();
	createGlobalSetLayout();
	createMaterialSetLayout();
	createObjectDataSetLayout();
	createDescriptorPool();
	loadPipelineCache();
	createGraphicsPipeline();
	createUniformBuffers();
	createObjectDataBuffer();
	createIndirectBuffer();

	// Initialize ResourceManager with material descriptor resources BEFORE createDescriptorSets
	// This ensures default textures exist for IBL placeholder bindings
	_resourceManager.initMaterialResources(*_materialSetLayout, _descriptorPool);

	createDescriptorSets();
	createSyncObjects();

	LogSystem::get().info("Renderer initialized successfully");
}

Renderer::~Renderer() {
	LogSystem::get().info("Shutting down Renderer...");
	savePipelineCache();
	_device->waitIdle();

	// Clear GPU resources from ResourceManager before destroying the descriptor pool
	// This prevents validation errors about freeing descriptor sets from an invalid pool
	_resourceManager.clearGPUResources();

	freeCommandBuffers();
	_swapChain.reset();

	_unifiedVertexBuffer.reset();
	_unifiedIndexBuffer.reset();

	// Destroy depth image with VMA
	if (_depthAllocation) {
		vmaDestroyImage(_device.getVmaAllocator(), _depthImage, _depthAllocation);
	}
}

void Renderer::render(const Model& model, std::function<void(VkCommandBuffer)> uiRenderCallback) {
	if (!beginFrame()) {
		return; // SwapChain was recreated, skip this frame
	}

	const auto& cmd = _frames[_currentFrameIndex].commandBuffer;

	// Update uniform buffer with current MVP matrices
	updateUniformBuffer(_currentFrameIndex);

	// Transition image to COLOR_ATTACHMENT_OPTIMAL
	VulkanUtils::transitionImage(*cmd, _swapChain->getImages()[_currentImageIndex], vk::ImageLayout::eUndefined,
								 vk::ImageLayout::eColorAttachmentOptimal, {},
								 vk::AccessFlagBits2::eColorAttachmentWrite, vk::PipelineStageFlagBits2::eTopOfPipe,
								 vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::ImageAspectFlagBits::eColor);

	// Begin dynamic rendering
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

	// Bind pipeline
	_graphicsPipeline->bind(cmd);

	// Set viewport and scissor
	cmd.setViewport(
		0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f));
	cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));

	// Bind global descriptor set (Set 0) once - it doesn't change per-primitive
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_graphicsPipeline->getLayout(), 0,
						   *_frames[_currentFrameIndex].descriptorSet, nullptr);

	// Bind object data SSBO (Set 2)
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_graphicsPipeline->getLayout(), 2,
						   *_frames[_currentFrameIndex].objectDataSet, nullptr);

	// Update object data SSBO with per-object transforms
	updateObjectData(model);

	// Build draw batches (sorts by material) and upload indirect commands
	buildDrawBatches(model);

	// Render using indirect draw calls
	renderIndirect(cmd);

	cmd.endRendering();

	// UI Rendering Pass (if callback provided)
	if (uiRenderCallback) {
		// Start UI rendering pass (no depth, load previous color)
		vk::RenderingAttachmentInfo uiColorAttachment{.imageView = *_swapChainImageViews[_currentImageIndex],
													  .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
													  .loadOp = vk::AttachmentLoadOp::eLoad,
													  .storeOp = vk::AttachmentStoreOp::eStore};

		vk::RenderingInfo uiRenderingInfo{.renderArea = vk::Rect2D{{0, 0}, extent},
										  .layerCount = 1,
										  .colorAttachmentCount = 1,
										  .pColorAttachments = &uiColorAttachment};

		cmd.beginRendering(uiRenderingInfo);

		// Call the UI render callback with raw VkCommandBuffer
		uiRenderCallback(static_cast<VkCommandBuffer>(*cmd));

		cmd.endRendering();
	}

	// Transition image to PRESENT_SRC
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
	_commandPool = &_device.getTransferCommandPool();

	vk::CommandBufferAllocateInfo allocInfo{.commandPool = *_commandPool->getCommandPool(),
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
	_commandPool = nullptr;
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

void Renderer::createMaterialSetLayout() {
	// Material set layout matching glTF PBR with extensions:
	// Binding 0: MaterialUBO (uniform buffer)
	// Binding 1: baseColorMap
	// Binding 2: metallicRoughnessMap
	// Binding 3: normalMap
	// Binding 4: occlusionMap
	// Binding 5: emissiveMap
	// Binding 6: clearcoatMap
	// Binding 7: clearcoatRoughnessMap
	// Binding 8: clearcoatNormalMap
	// Binding 9: transmissionMap
	std::vector<vk::DescriptorSetLayoutBinding> bindings = {
		{.binding = 0,
		 .descriptorType = vk::DescriptorType::eUniformBuffer,
		 .descriptorCount = 1,
		 .stageFlags = vk::ShaderStageFlagBits::eFragment},
	};

	// 9 texture bindings (1-9)
	for (uint32_t i = 1; i <= 9; i++) {
		bindings.push_back({.binding = i,
							.descriptorType = vk::DescriptorType::eCombinedImageSampler,
							.descriptorCount = 1,
							.stageFlags = vk::ShaderStageFlagBits::eFragment});
	}

	vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = static_cast<uint32_t>(bindings.size()),
												 .pBindings = bindings.data()};

	_materialSetLayout = vk::raii::DescriptorSetLayout(*_device, layoutInfo);
}

void Renderer::createObjectDataSetLayout() {
	// Set 2: Object Data SSBO for per-object transforms
	vk::DescriptorSetLayoutBinding ssboBinding{.binding = 0,
											   .descriptorType = vk::DescriptorType::eStorageBuffer,
											   .descriptorCount = 1,
											   .stageFlags = vk::ShaderStageFlagBits::eVertex};

	vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = 1, .pBindings = &ssboBinding};

	_objectDataSetLayout = vk::raii::DescriptorSetLayout(*_device, layoutInfo);
	LogSystem::get().trace("Created object data SSBO descriptor set layout");
}

void Renderer::createObjectDataBuffer() {
	// Use header constant for initial buffer size
	vk::DeviceSize bufferSize = sizeof(ObjectData) * INITIAL_MAX_OBJECTS;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_frames[i].objectDataBuffer = std::make_unique<VulkanBuffer>(
			_device, bufferSize, vk::BufferUsageFlagBits::eStorageBuffer,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		// Persistent mapping
		_frames[i].objectDataBuffer->map();
	}
	LogSystem::get().trace("Created object data SSBO buffers ({} bytes each)", bufferSize);
}

void Renderer::updateObjectData(const Model& model) {
	// Build object data for all primitives
	std::vector<ObjectData> objectData;
	objectData.reserve(model.getPrimitives().size());

	// Base transform: Rotate +90 degrees around X to fix model orientation (glTF uses Y-up)
	glm::mat4 fixRotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	for (size_t i = 0; i < model.getPrimitives().size(); i++) {
		// For now, all primitives share the same transform
		// Future: each primitive could have its own transform from scene graph
		objectData.push_back(ObjectData::fromModelMatrix(fixRotation, static_cast<uint32_t>(i)));
	}

	// Upload to current frame's SSBO
	if (!objectData.empty()) {
		_frames[_currentFrameIndex].objectDataBuffer->upload(objectData.data(), objectData.size() * sizeof(ObjectData));
	}
}

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

void Renderer::buildUnifiedBuffers(const Model& model) {
	// Collect all vertex and index data into unified buffers
	std::vector<Vertex> allVertices;
	std::vector<uint32_t> allIndices;
	_meshRegions.clear();
	_meshRegions.reserve(model.getPrimitives().size());

	for (const auto& prim : model.getPrimitives()) {
		if (!prim.mesh) {
			_meshRegions.push_back({0, 0, 0}); // Placeholder for skipped primitives
			continue;
		}

		const auto& vertices = prim.mesh->getVertices();
		const auto& indices = prim.mesh->getIndices();

		MeshRegion region{
			.firstIndex = static_cast<uint32_t>(allIndices.size()),
			.indexCount = static_cast<uint32_t>(indices.size()),
			.vertexOffset = static_cast<int32_t>(allVertices.size()),
		};
		_meshRegions.push_back(region);

		// Append vertices and indices
		allVertices.insert(allVertices.end(), vertices.begin(), vertices.end());
		allIndices.insert(allIndices.end(), indices.begin(), indices.end());
	}

	if (allVertices.empty() || allIndices.empty()) {
		return;
	}

	// Create unified vertex buffer
	vk::DeviceSize vertexBufferSize = allVertices.size() * sizeof(Vertex);
	_unifiedVertexBuffer = std::make_unique<VulkanBuffer>(
		_device, vertexBufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Create unified index buffer
	vk::DeviceSize indexBufferSize = allIndices.size() * sizeof(uint32_t);
	_unifiedIndexBuffer = std::make_unique<VulkanBuffer>(
		_device, indexBufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Upload via staging buffer
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

	// Copy using one-time command buffer
	VulkanUtils::executeImmediate(_device, [&](auto& cmd) {
		cmd.copyBuffer(stagingVertex.getBuffer(), _unifiedVertexBuffer->getBuffer(),
					   vk::BufferCopy{.size = vertexBufferSize});
		cmd.copyBuffer(stagingIndex.getBuffer(), _unifiedIndexBuffer->getBuffer(),
					   vk::BufferCopy{.size = indexBufferSize});
	});

	_unifiedBuffersDirty = false;
	LogSystem::get().info("Built unified buffers: {} vertices, {} indices", allVertices.size(), allIndices.size());
}

void Renderer::buildDrawBatches(const Model& model) {
	// Build unified buffers on first call or when model changes
	if (_unifiedBuffersDirty) {
		buildUnifiedBuffers(model);
	}

	// Clear previous frame's data
	_drawBatches.clear();
	_indirectCommands.clear();

	const auto& primitives = model.getPrimitives();
	if (primitives.empty() || _meshRegions.empty()) {
		return;
	}

	// Build a list of (primitiveIndex, material) for sorting
	struct PrimitiveInfo {
		uint32_t index;
		GPUMaterial* material;
	};
	std::vector<PrimitiveInfo> sortedPrimitives;
	sortedPrimitives.reserve(primitives.size());

	for (uint32_t i = 0; i < primitives.size(); i++) {
		const auto& prim = primitives[i];
		if (!prim.mesh || _meshRegions[i].indexCount == 0)
			continue;

		GPUMaterial* gpuMaterial = nullptr;
		if (prim.material) {
			gpuMaterial = _resourceManager.getOrCreateGPUMaterial(prim.material.get());
		}

		sortedPrimitives.push_back({i, gpuMaterial});
	}

	// Sort by material pointer (group same materials together)
	std::sort(sortedPrimitives.begin(), sortedPrimitives.end(),
			  [](const PrimitiveInfo& a, const PrimitiveInfo& b) { return a.material < b.material; });

	// Build batches and indirect commands
	GPUMaterial* currentMaterial = nullptr;

	for (const auto& prim : sortedPrimitives) {
		// Start a new batch when material changes
		if (prim.material != currentMaterial) {
			currentMaterial = prim.material;
			_drawBatches.push_back({
				.material = currentMaterial,
				.firstCommand = static_cast<uint32_t>(_indirectCommands.size()),
				.commandCount = 0,
			});
		}

		// Get mesh region for this primitive
		const auto& region = _meshRegions[prim.index];

		// Add indirect command with proper offsets into unified buffers
		vk::DrawIndexedIndirectCommand cmd{
			.indexCount = region.indexCount,
			.instanceCount = 1,
			.firstIndex = region.firstIndex,	 // Offset into unified index buffer
			.vertexOffset = region.vertexOffset, // Offset into unified vertex buffer
			.firstInstance = prim.index,		 // Used for SSBO lookup
		};
		_indirectCommands.push_back(cmd);
		_drawBatches.back().commandCount++;
	}

	// Upload indirect commands to GPU
	if (!_indirectCommands.empty()) {
		_frames[_currentFrameIndex].indirectBuffer->upload(
			_indirectCommands.data(), _indirectCommands.size() * sizeof(vk::DrawIndexedIndirectCommand));
	}
}

void Renderer::renderIndirect(const vk::raii::CommandBuffer& cmd) {
	if (!_unifiedVertexBuffer || !_unifiedIndexBuffer || _drawBatches.empty()) {
		return;
	}

	// Bind unified vertex and index buffers ONCE for all draw calls
	cmd.bindVertexBuffers(0, _unifiedVertexBuffer->getBuffer(), {0});
	cmd.bindIndexBuffer(_unifiedIndexBuffer->getBuffer(), 0, vk::IndexType::eUint32);

	// Render using indirect draw calls, one per batch (material group)
	for (const auto& batch : _drawBatches) {
		// Bind material descriptor set (Set 1)
		if (batch.material) {
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_graphicsPipeline->getLayout(), 1,
								   *batch.material->descriptorSet, nullptr);
		}

		// Issue indirect draw call for all commands in this batch
		// With unified buffers, this draws all same-material objects efficiently!
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
		.setLayout({*_globalSetLayout, *_materialSetLayout, *_objectDataSetLayout}, {})
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

void Renderer::createDescriptorSets() {
	createGlobalDescriptorSets();
	createObjectDataDescriptorSets();
}

void Renderer::createGlobalDescriptorSets() {
	// Allocate global descriptor sets (Set 0: UBO + IBL textures)
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
}

void Renderer::createObjectDataDescriptorSets() {
	// Allocate object data descriptor sets (Set 2: SSBO for per-object transforms)
	std::vector<vk::DescriptorSetLayout> objectDataLayouts(MAX_FRAMES_IN_FLIGHT, *_objectDataSetLayout);

	vk::DescriptorSetAllocateInfo objectDataAllocInfo{.descriptorPool = *_descriptorPool,
													  .descriptorSetCount =
														  static_cast<uint32_t>(objectDataLayouts.size()),
													  .pSetLayouts = objectDataLayouts.data()};

	auto objectDataSets = vk::raii::DescriptorSets(*_device, objectDataAllocInfo);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_frames[i].objectDataSet = std::move(objectDataSets[i]);

		// SSBO descriptor
		vk::DescriptorBufferInfo ssboInfo{
			.buffer = _frames[i].objectDataBuffer->getBuffer(), .offset = 0, .range = VK_WHOLE_SIZE};

		vk::WriteDescriptorSet ssboWrite{
			.dstSet = *_frames[i].objectDataSet,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.pBufferInfo = &ssboInfo,
		};

		_device->updateDescriptorSets(ssboWrite, {});
	}
}

void Renderer::updateUniformBuffer(uint32_t frameIndex) {
	auto extent = _swapChain->getExtent();
	UniformBufferObject ubo{};

	// Get camera matrices from Camera class
	ubo.view = _camera.getViewMatrix();
	ubo.proj = _camera.getProjectionMatrix(static_cast<float>(extent.width) / static_cast<float>(extent.height));
	ubo.camPos = _camera.getPosition();

	// Lighting
	ubo.lightDir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
	ubo.lightColor = glm::vec3(1.0f, 1.0f, 1.0f) * 5.0f;

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

} // namespace Fishy