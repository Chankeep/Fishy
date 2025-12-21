#include "Renderer.h"
#include <iostream>

#include "PipelineBuilder.h"
#include "core/ResourceManager.h"
#include "core/VulkanDevice.h"
#include "core/Window.h"
#include "vulkan/vulkan.hpp"

#include <array>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace Fishy {

Renderer::Renderer(VulkanDevice& device, Window& window, ResourceManager& resourceManager)
	: _device(device), _window(window), _resourceManager(resourceManager) {
	_frames.resize(MAX_FRAMES_IN_FLIGHT);
	recreateSwapChain();
	createCommandBuffers();
	createGlobalSetLayout();
	createMaterialSetLayout();
	createDescriptorPool(); // Create pool before material

	_material = std::make_unique<Material>(
		_device,
		Material::Config{.albedo = _resourceManager.getTexture("assets/laminate-flooring-brown-bl/albedo.png"),
						 .metallic = _resourceManager.getTexture("assets/laminate-flooring-brown-bl/metallic.png"),
						 .roughness = _resourceManager.getTexture("assets/laminate-flooring-brown-bl/roughness.png"),
						 .normal = _resourceManager.getTexture("assets/laminate-flooring-brown-bl/normal-ogl.png"),
						 .ao = _resourceManager.getTexture("assets/laminate-flooring-brown-bl/ao.png"),
						 .height = _resourceManager.getTexture("assets/laminate-flooring-brown-bl/height.png")});
	_material->createDescriptorSet(*_descriptorPool, *_materialSetLayout);

	createGraphicsPipeline();
	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffers();
	createDescriptorSets(); // Now only for Global sets
	createSyncObjects();
}

Renderer::~Renderer() {
	_device->waitIdle();
	freeCommandBuffers();
	_swapChain.reset();
}

void Renderer::recreateSwapChain() {
	auto extent = _window.getExtent();
	while (extent.width == 0 || extent.height == 0) {
		extent = _window.getExtent();
		glfwWaitEvents();
	}

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
	// We need these to be 1:1 with images because the presentation engine holds them
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
	// Binding 0: Albedo
	// Binding 1: Metallic
	// Binding 2: Roughness
	// Binding 3: Normal
	// Binding 4: AO
	// Binding 5: Height
	// std::vector<vk::DescriptorSetLayoutBinding> bindings = {
	// 	{.binding = 0,
	// 	 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
	// 	 .descriptorCount = 1,
	// 	 .stageFlags = vk::ShaderStageFlagBits::eFragment},
	// 	{.binding = 1,
	// 	 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
	// 	 .descriptorCount = 1,
	// 	 .stageFlags = vk::ShaderStageFlagBits::eFragment},
	// 	{.binding = 2,
	// 	 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
	// 	 .descriptorCount = 1,
	// 	 .stageFlags = vk::ShaderStageFlagBits::eFragment},
	// 	{.binding = 3,
	// 	 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
	// 	 .descriptorCount = 1,
	// 	 .stageFlags = vk::ShaderStageFlagBits::eFragment},
	// 	{.binding = 4,
	// 	 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
	// 	 .descriptorCount = 1,
	// 	 .stageFlags = vk::ShaderStageFlagBits::eFragment},
	// 	{.binding = 5,
	// 	 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
	// 	 .descriptorCount = 1,
	// 	 .stageFlags = vk::ShaderStageFlagBits::eFragment}};

	std::vector<vk::DescriptorSetLayoutBinding> bindings;
	for (uint32_t i = 0; i < 6; i++) {
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
	const auto& shaderModule = _resourceManager.getShader("shaders/shader.slang.spv");

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
}

void Renderer::createVertexBuffer() {
	vk::DeviceSize bufferSize = sizeof(_vertices[0]) * _vertices.size();

	// Create Device Local Buffer
	_vertexBuffer = std::make_unique<VulkanBuffer>(
		_device, bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Upload using Staging Buffer
	_vertexBuffer->uploadStaged(_commandPool, _device.getGraphicsQueue(), (void*)_vertices.data(), bufferSize);
}

void Renderer::createIndexBuffer() {
	vk::DeviceSize bufferSize = sizeof(_indices[0]) * _indices.size();

	// Create Device Local Buffer
	_indexBuffer = std::make_unique<VulkanBuffer>(
		_device, bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
		vk::MemoryPropertyFlagBits::eDeviceLocal);

	// Upload using Staging Buffer
	_indexBuffer->uploadStaged(_commandPool, _device.getGraphicsQueue(), (void*)_indices.data(), bufferSize);
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
							   .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)},
		vk::DescriptorPoolSize{.type = vk::DescriptorType::eCombinedImageSampler,
							   .descriptorCount =
								   static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 10)}}; // More samplers for materials

	vk::DescriptorPoolCreateInfo poolInfo{.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
										  .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
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

	// Lighting
	ubo.camPos = glm::vec3(2.0f, 2.0f, 2.0f); // Should match camera pos
	ubo.lightDir = glm::normalize(glm::vec3(1.0f, 1.0f, 2.0f));
	ubo.lightColor = glm::vec3(1.0f, 1.0f, 1.0f) * 5.0f; // High intensity for PBR

	// Model
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(ubo.camPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f),
								static_cast<float>(extent.width) / static_cast<float>(extent.height), 0.1f, 10.0f);
	ubo.proj[1][1] *= -1; // Invert Y for Vulkan

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

void Renderer::transitionImageLayout(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
									 vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
									 vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
									 vk::ImageAspectFlags aspectMask) {
	transitionImage(*_frames[_currentFrameIndex].commandBuffer, _swapChain->getImages()[imageIndex], oldLayout,
					newLayout, srcAccessMask, dstAccessMask, srcStageMask, dstStageMask, aspectMask);
}

const vk::raii::CommandBuffer& Renderer::BeginFrame() {
	if (_isFrameStarted) {
		throw std::runtime_error("Can't call BeginFrame while already in progress");
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
		static vk::raii::CommandBuffer nullBuffer(nullptr);
		return nullBuffer;
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

	return _frames[_currentFrameIndex].commandBuffer;
}

void Renderer::EndFrame() {
	if (!_isFrameStarted) {
		throw std::runtime_error("Can't call EndFrame if frame not started");
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
			return format;
		} else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw std::runtime_error("failed to find supported depth format!");
}

void Renderer::createDepthResources() {
	_depthFormat = findDepthFormat();
	vk::Extent2D extent = getSwapChainExtent();

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
	// This is required for Dynamic Rendering as we don't have implicit subpass transitions on first use.
	{
		vk::CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eTransient,
										   .queueFamilyIndex = _device.getGraphicsQueueFamilyIndex()};
		vk::raii::CommandPool commandPool(*_device, poolInfo);

		vk::CommandBufferAllocateInfo allocInfo{
			.commandPool = *commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};
		auto commandBuffers = vk::raii::CommandBuffers(*_device, allocInfo);
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