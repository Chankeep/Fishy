#include "Renderer.h"
#include <iostream>

#include "PipelineBuilder.h"
#include "core/VulkanDevice.h"
#include "core/Window.h"
#include "resources/ResourceManager.h"
#include "vulkan/vulkan.hpp"

#include <array>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace Fishy {

Renderer::Renderer(VulkanDevice& device, Window& window, ResourceManager& resourceManager)
	: _device(device), _window(window), _resourceManager(resourceManager) {
	LogSystem::get().info("Initializing Renderer...");
	_frames.resize(MAX_FRAMES_IN_FLIGHT);
	recreateSwapChain();
	createCommandBuffers();
	createGlobalSetLayout();
	createMaterialSetLayout();
	createDescriptorPool();
	createGraphicsPipeline();
	createUniformBuffers();
	createDescriptorSets();
	createSyncObjects();

	// Initialize ResourceManager with material descriptor resources
	_resourceManager.initMaterialResources(*_materialSetLayout, _descriptorPool);
	LogSystem::get().info("Renderer initialized successfully");
}

Renderer::~Renderer() {
	LogSystem::get().info("Shutting down Renderer...");
	_device->waitIdle();

	// Clear GPU resources from ResourceManager before destroying the descriptor pool
	// This prevents validation errors about freeing descriptor sets from an invalid pool
	_resourceManager.clearGPUResources();

	freeCommandBuffers();
	_swapChain.reset();
}

void Renderer::render(const Model& model, std::function<void(VkCommandBuffer)> uiRenderCallback) {
	if (!beginFrame()) {
		return; // SwapChain was recreated, skip this frame
	}

	const auto& cmd = _frames[_currentFrameIndex].commandBuffer;

	// Update uniform buffer with current MVP matrices
	updateUniformBuffer(_currentFrameIndex);

	// Transition image to COLOR_ATTACHMENT_OPTIMAL
	transitionImage(*cmd, _swapChain->getImages()[_currentImageIndex], vk::ImageLayout::eUndefined,
					vk::ImageLayout::eColorAttachmentOptimal, {}, vk::AccessFlagBits2::eColorAttachmentWrite,
					vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
					vk::ImageAspectFlagBits::eColor);

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

	// Render each primitive in the model
	for (const auto& primitive : model.getPrimitives()) {
		if (!primitive.mesh) {
			continue;
		}

		// Get or create GPU resources
		GPUMesh* gpuMesh = _resourceManager.getOrCreateGPUMesh(primitive.mesh.get(), _commandPool);
		GPUMaterial* gpuMaterial = nullptr;

		if (primitive.material) {
			gpuMaterial = _resourceManager.getOrCreateGPUMaterial(primitive.material.get());
		}

		if (!gpuMesh) {
			continue;
		}

		// Bind vertex and index buffers
		cmd.bindVertexBuffers(0, *gpuMesh->vertexBuffer->getBuffer(), {0});
		cmd.bindIndexBuffer(*gpuMesh->indexBuffer->getBuffer(), 0, vk::IndexType::eUint32);

		// Bind descriptor sets
		std::vector<vk::DescriptorSet> descriptorSets;
		descriptorSets.push_back(*_frames[_currentFrameIndex].descriptorSet);

		if (gpuMaterial) {
			descriptorSets.push_back(*gpuMaterial->descriptorSet);
		}

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *_graphicsPipeline->getLayout(), 0, descriptorSets,
							   nullptr);

		// Draw indexed
		cmd.drawIndexed(gpuMesh->indexCount, 1, 0, 0, 0);
	}

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
	transitionImage(*cmd, _swapChain->getImages()[_currentImageIndex], vk::ImageLayout::eColorAttachmentOptimal,
					vk::ImageLayout::ePresentSrcKHR, vk::AccessFlagBits2::eColorAttachmentWrite, {},
					vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eBottomOfPipe,
					vk::ImageAspectFlagBits::eColor);

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
	vk::CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
									   .queueFamilyIndex = _device.getGraphicsQueueFamilyIndex()};
	_commandPool = vk::raii::CommandPool(*_device, poolInfo);

	vk::CommandBufferAllocateInfo allocInfo{.commandPool = *_commandPool,
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
	vk::DescriptorSetLayoutBinding uboLayoutBinding{.binding = 0,
													.descriptorType = vk::DescriptorType::eUniformBuffer,
													.descriptorCount = 1,
													.stageFlags = vk::ShaderStageFlagBits::eVertex |
																  vk::ShaderStageFlagBits::eFragment};

	vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = 1, .pBindings = &uboLayoutBinding};

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
		.setLayout({*_globalSetLayout, *_materialSetLayout}, {})
		.setRenderingFormats({_swapChain->getFormat()}, _depthFormat)
		.setDepthStencilTest(true, true, vk::CompareOp::eLess, false, vk::CompareOp::eAlways);

	_graphicsPipeline = builder.build(*_device);
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
	std::array<vk::DescriptorPoolSize, 2> poolSizes{
		vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer,
							   .descriptorCount =
								   static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT + 100)}, // Extra for materials
		vk::DescriptorPoolSize{.type = vk::DescriptorType::eCombinedImageSampler,
							   .descriptorCount =
								   static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 10 + 500)}}; // Extra for materials

	vk::DescriptorPoolCreateInfo poolInfo{.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
										  .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT + 100),
										  .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
										  .pPoolSizes = poolSizes.data()};

	_descriptorPool = vk::raii::DescriptorPool(*_device, poolInfo);
}

void Renderer::createDescriptorSets() {
	std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *_globalSetLayout);

	vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *_descriptorPool,
											.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
											.pSetLayouts = layouts.data()};

	auto descriptorSets = vk::raii::DescriptorSets(*_device, allocInfo);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_frames[i].descriptorSet = std::move(descriptorSets[i]);

		vk::DescriptorBufferInfo bufferInfo{
			.buffer = *_frames[i].uniformBuffer->getBuffer(), .offset = 0, .range = sizeof(UniformBufferObject)};

		std::array<vk::WriteDescriptorSet, 1> descriptorWrites{};

		// Uniform Buffer
		descriptorWrites[0].dstSet = *_frames[i].descriptorSet;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = vk::DescriptorType::eUniformBuffer;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		_device->updateDescriptorSets(descriptorWrites, {});
	}
}

void Renderer::updateUniformBuffer(uint32_t frameIndex) {
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	auto extent = _swapChain->getExtent();
	UniformBufferObject ubo{};

	// Camera position (Y-up coordinate system for glTF)
	ubo.camPos = glm::vec3(0.0f, 0.0f, 3.0f);

	// Lighting
	ubo.lightDir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
	ubo.lightColor = glm::vec3(1.0f, 1.0f, 1.0f) * 5.0f;

	// Model - rotate around Y axis (glTF uses Y-up)
	// Rotate +90 degrees around X to fix model orientation
	glm::mat4 fixRotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	glm::mat4 animRotation = glm::rotate(glm::mat4(1.0f), time * glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	ubo.model = animRotation * fixRotation;

	// View - Y-up coordinate system
	ubo.view = glm::lookAt(ubo.camPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	ubo.proj = glm::perspective(glm::radians(45.0f),
								static_cast<float>(extent.width) / static_cast<float>(extent.height), 0.1f, 100.0f);
	ubo.proj[1][1] *= -1; // Invert Y for Vulkan

	// Debug settings
	ubo.debugViewInputs = _debugViewInputs;
	ubo.debugViewEquation = _debugViewEquation;

	_frames[frameIndex].uniformBuffer->upload(&ubo, sizeof(ubo));
}

void Renderer::transitionImage(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout oldLayout,
							   vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask,
							   vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask,
							   vk::PipelineStageFlags2 dstStageMask, vk::ImageAspectFlags aspectMask) {
	vk::ImageMemoryBarrier2 barrier{
		.srcStageMask = srcStageMask,
		.srcAccessMask = srcAccessMask,
		.dstStageMask = dstStageMask,
		.dstAccessMask = dstAccessMask,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = {
			.aspectMask = aspectMask, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

	vk::DependencyInfo dependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier};

	cmd.pipelineBarrier2(dependencyInfo);
}

bool Renderer::beginFrame() {
	if (_isFrameStarted) {
		throw std::runtime_error("Can't call beginFrame while already in progress");
	}

	auto result = _device->waitForFences(*_frames[_currentFrameIndex].inFlightFence, vk::True, UINT64_MAX);
	if (result != vk::Result::eSuccess) {
		throw std::runtime_error("WaitForFences failed");
	}

	vk::Result acquireResult;
	uint32_t imageIndex;
	try {
		auto [result, idx] = _swapChain->get().acquireNextImage(
			UINT64_MAX, *_frames[_currentFrameIndex].imageAvailableSemaphore, nullptr);
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

	vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
	vk::CommandBuffer rawCmd = *_frames[_currentFrameIndex].commandBuffer;

	vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
							  .pWaitSemaphores = &*_frames[_currentFrameIndex].imageAvailableSemaphore,
							  .pWaitDstStageMask = waitStages,
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

float Renderer::getAspectRatio() const { return _swapChain->getExtent().width / (float)_swapChain->getExtent().height; }

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
	_depthFormat = findDepthFormat();
	vk::Extent2D extent = getSwapChainExtent();

	LogSystem::get().info("Creating depth resources: {}x{} format:{}",
		extent.width, extent.height, vk::to_string(_depthFormat));

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

	_depthImage = vk::raii::Image(*_device, imageInfo);

	vk::MemoryRequirements memRequirements = _depthImage.getMemoryRequirements();

	vk::MemoryAllocateInfo allocInfo{.allocationSize = memRequirements.size,
									 .memoryTypeIndex = _device.findMemoryType(
										 memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)};

	_depthImageMemory = vk::raii::DeviceMemory(*_device, allocInfo);
	_depthImage.bindMemory(*_depthImageMemory, 0);

	vk::ImageViewCreateInfo viewInfo{.image = *_depthImage,
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

	// Perform explicit layout transition Undefined -> DepthStencilAttachmentOptimal
	{
		vk::CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eTransient,
										   .queueFamilyIndex = _device.getGraphicsQueueFamilyIndex()};
		vk::raii::CommandPool commandPool(*_device, poolInfo);

		vk::CommandBufferAllocateInfo cmdAllocInfo{
			.commandPool = *commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};
		auto commandBuffers = vk::raii::CommandBuffers(*_device, cmdAllocInfo);
		vk::raii::CommandBuffer& cmd = commandBuffers[0];

		vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
		cmd.begin(beginInfo);

		transitionImage(
			*cmd, *_depthImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal,
			vk::AccessFlagBits2::eNone,
			vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eTopOfPipe,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			viewInfo.subresourceRange.aspectMask);

		cmd.end();

		vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*cmd};
		_device.getGraphicsQueue().submit(submitInfo, nullptr);
		_device.getGraphicsQueue().waitIdle();
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

} // namespace Fishy