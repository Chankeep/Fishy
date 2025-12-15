#include "Renderer.h"
#include <iostream>

#include "PipelineBuilder.h"
#include "core/ResourceManager.h"
#include "core/SwapChain.h"
#include "core/VulkanDevice.h"
#include "core/Window.h"

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
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
}

Renderer::~Renderer() {
	_device.getDevice().waitIdle();
	freeCommandBuffers();
	_swapChain.reset();
}

void Renderer::recreateSwapChain() {
	auto extent = _window.getExtent();
	while (extent.width == 0 || extent.height == 0) {
		extent = _window.getExtent();
		glfwWaitEvents();
	}

	_device.getDevice().waitIdle();

	if (_swapChain == nullptr) {
		_swapChain = std::make_unique<SwapChain>(_device.getDevice(), _device.getPhysicalDevice(), _window.getSurface(),
												 extent.width, extent.height);
	} else {
		_swapChain->recreate(extent.width, extent.height);
	}

	createSyncObjects();
}

void Renderer::createCommandBuffers() {
	vk::CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
									   .queueFamilyIndex = _device.getGraphicsQueueFamilyIndex()};
	_commandPool = vk::raii::CommandPool(_device.getDevice(), poolInfo);

	vk::CommandBufferAllocateInfo allocInfo{.commandPool = *_commandPool,
											.level = vk::CommandBufferLevel::ePrimary,
											.commandBufferCount = MAX_FRAMES_IN_FLIGHT};

	auto commandBuffers = vk::raii::CommandBuffers(_device.getDevice(), allocInfo);
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
		_frames[i].imageAvailableSemaphore = vk::raii::Semaphore(_device.getDevice(), vk::SemaphoreCreateInfo{});
		_frames[i].inFlightFence =
			vk::raii::Fence(_device.getDevice(), vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
	}

	// Per-swapchain-image sync objects
	// We need these to be 1:1 with images because the presentation engine holds them
	_renderFinishedSemaphores.clear();
	size_t imageCount = _swapChain->getImageCount();
	for (size_t i = 0; i < imageCount; i++) {
		_renderFinishedSemaphores.emplace_back(_device.getDevice(), vk::SemaphoreCreateInfo{});
	}
}

void Renderer::createDescriptorSetLayout() {
	vk::DescriptorSetLayoutBinding uboLayoutBinding{.binding = 0,
													.descriptorType = vk::DescriptorType::eUniformBuffer,
													.descriptorCount = 1,
													.stageFlags = vk::ShaderStageFlagBits::eVertex};

	vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = 1, .pBindings = &uboLayoutBinding};

	_descriptorSetLayout = vk::raii::DescriptorSetLayout(_device.getDevice(), layoutInfo);
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

	PipelineBuilder builder(*_device.getDevice());

	builder.setShaders(shaderModule, shaderModule, "vertMain", "fragMain")
		.setVertexInput(vertexInputInfo)
		.setInputTopology(vk::PrimitiveTopology::eTriangleList)
		.setCullMode(vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise)
		.setLayout({*_descriptorSetLayout}, {})
		.setRenderingFormats({_swapChain->getFormat()}, vk::Format::eUndefined);

	_graphicsPipeline = builder.build(_device.getDevice());
}

void Renderer::createVertexBuffer() {
	vk::DeviceSize bufferSize = sizeof(_vertices[0]) * _vertices.size();

	vk::raii::Buffer stagingBuffer(nullptr);
	vk::raii::DeviceMemory stagingBufferMemory(nullptr);
	createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
				 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer,
				 stagingBufferMemory);

	void* data = stagingBufferMemory.mapMemory(0, bufferSize);
	memcpy(data, _vertices.data(), bufferSize);
	stagingBufferMemory.unmapMemory();

	createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
				 vk::MemoryPropertyFlagBits::eDeviceLocal, _vertexBuffer, _vertexBufferMemory);

	copyBuffer(stagingBuffer, _vertexBuffer, bufferSize);
}

void Renderer::createIndexBuffer() {
	vk::DeviceSize bufferSize = sizeof(_indices[0]) * _indices.size();

	vk::raii::Buffer stagingBuffer(nullptr);
	vk::raii::DeviceMemory stagingBufferMemory(nullptr);
	createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
				 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer,
				 stagingBufferMemory);

	void* data = stagingBufferMemory.mapMemory(0, bufferSize);
	memcpy(data, _indices.data(), bufferSize);
	stagingBufferMemory.unmapMemory();

	createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
				 vk::MemoryPropertyFlagBits::eDeviceLocal, _indexBuffer, _indexBufferMemory);

	copyBuffer(stagingBuffer, _indexBuffer, bufferSize);
}

void Renderer::createUniformBuffers() {
	vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vk::raii::Buffer buffer(nullptr);
		vk::raii::DeviceMemory bufferMem(nullptr);

		createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
					 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer,
					 bufferMem);

		_frames[i].uniformBuffer = std::move(buffer);
		_frames[i].uniformBufferMemory = std::move(bufferMem);
		_frames[i].uniformBufferMapped = _frames[i].uniformBufferMemory.mapMemory(0, bufferSize);
	}
}

void Renderer::createDescriptorPool() {
	vk::DescriptorPoolSize poolSize{.type = vk::DescriptorType::eUniformBuffer,
									.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};

	vk::DescriptorPoolCreateInfo poolInfo{.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
										  .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
										  .poolSizeCount = 1,
										  .pPoolSizes = &poolSize};

	_descriptorPool = vk::raii::DescriptorPool(_device.getDevice(), poolInfo);
}

void Renderer::createDescriptorSets() {
	std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *_descriptorSetLayout);

	vk::DescriptorSetAllocateInfo allocInfo{.descriptorPool = *_descriptorPool,
											.descriptorSetCount = static_cast<uint32_t>(layouts.size()),
											.pSetLayouts = layouts.data()};

	auto descriptorSets = _device.getDevice().allocateDescriptorSets(allocInfo);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		_frames[i].descriptorSet = std::move(descriptorSets[i]);

		vk::DescriptorBufferInfo bufferInfo{
			.buffer = *_frames[i].uniformBuffer, .offset = 0, .range = sizeof(UniformBufferObject)};

		vk::WriteDescriptorSet descriptorWrite{.dstSet = *_frames[i].descriptorSet,
											   .dstBinding = 0,
											   .dstArrayElement = 0,
											   .descriptorCount = 1,
											   .descriptorType = vk::DescriptorType::eUniformBuffer,
											   .pBufferInfo = &bufferInfo};

		_device.getDevice().updateDescriptorSets(descriptorWrite, {});
	}
}

void Renderer::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
							vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory) {
	vk::BufferCreateInfo bufferInfo{.size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};

	buffer = vk::raii::Buffer(_device.getDevice(), bufferInfo);

	vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
	vk::MemoryAllocateInfo allocInfo{.allocationSize = memRequirements.size,
									 .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};

	bufferMemory = vk::raii::DeviceMemory(_device.getDevice(), allocInfo);
	buffer.bindMemory(*bufferMemory, 0);
}

void Renderer::copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size) {
	vk::CommandBufferAllocateInfo allocInfo{
		.commandPool = *_commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};

	auto commandBuffers = _device.getDevice().allocateCommandBuffers(allocInfo);
	auto& commandBuffer = commandBuffers.front();

	vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
	commandBuffer.begin(beginInfo);
	commandBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{0, 0, size});
	commandBuffer.end();

	vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};
	_device.getGraphicsQueue().submit(submitInfo, nullptr);
	_device.getGraphicsQueue().waitIdle();
}

uint32_t Renderer::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
	vk::PhysicalDeviceMemoryProperties memProperties = _device.getPhysicalDevice().getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

void Renderer::updateUniformBuffer(uint32_t frameIndex) {
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	auto extent = _swapChain->getExtent();
	UniformBufferObject ubo{};
	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f),
								static_cast<float>(extent.width) / static_cast<float>(extent.height), 0.1f, 10.0f);
	ubo.proj[1][1] *= -1; // Invert Y for Vulkan

	memcpy(_frames[frameIndex].uniformBufferMapped, &ubo, sizeof(ubo));
}

void Renderer::transitionImageLayout(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
									 vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
									 vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask) {
	vk::ImageMemoryBarrier2 barrier{.srcStageMask = srcStageMask,
									.srcAccessMask = srcAccessMask,
									.dstStageMask = dstStageMask,
									.dstAccessMask = dstAccessMask,
									.oldLayout = oldLayout,
									.newLayout = newLayout,
									.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
									.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
									.image = _swapChain->getImages()[imageIndex],
									.subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
														 .baseMipLevel = 0,
														 .levelCount = 1,
														 .baseArrayLayer = 0,
														 .layerCount = 1}};

	vk::DependencyInfo dependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier};

	_frames[_currentFrameIndex].commandBuffer.pipelineBarrier2(dependencyInfo);
}

const vk::raii::CommandBuffer& Renderer::BeginFrame() {
	if (_isFrameStarted) {
		throw std::runtime_error("Can't call BeginFrame while already in progress");
	}

	auto result = _device.getDevice().waitForFences(*_frames[_currentFrameIndex].inFlightFence, vk::True, UINT64_MAX);
	if (result != vk::Result::eSuccess) {
		throw std::runtime_error("WaitForFences failed");
	}

	vk::Result acquireResult;
	uint32_t imageIndex;
	try {
		auto [result, idx] = _swapChain->getSwapChain().acquireNextImage(
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

	_device.getDevice().resetFences(*_frames[_currentFrameIndex].inFlightFence);

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
								   .pSwapchains = &*_swapChain->getSwapChain(),
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

const std::vector<vk::raii::ImageView>& Renderer::getSwapChainImageViews() const { return _swapChain->getImageViews(); }

vk::Extent2D Renderer::getSwapChainExtent() const { return _swapChain->getExtent(); }

vk::Format Renderer::getSwapChainFormat() const { return _swapChain->getFormat(); }

} // namespace Fishy