#pragma once

#include <volk.h>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include "GraphicsPipeline.h"
#include "Texture.h"
#include "Vertex.h"
#include "core/SwapChain.h"
#include "core/VulkanBuffer.h"

#include <memory>
#include <vector>

namespace Fishy {

class VulkanDevice;
class Window;
class SwapChain;
class ResourceManager;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

class Renderer {
public:
	Renderer(VulkanDevice& device, Window& window, ResourceManager& resourceManager);
	~Renderer();

	// Frame management
	const vk::raii::CommandBuffer& BeginFrame();
	void EndFrame();

	// Helpers for rendering
	void updateUniformBuffer(uint32_t frameIndex);
	static void transitionImage(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout oldLayout,
								vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask,
								vk::AccessFlags2 dstAccessMask, vk::PipelineStageFlags2 srcStageMask,
								vk::PipelineStageFlags2 dstStageMask, vk::ImageAspectFlags aspectMask);
	void transitionImageLayout(uint32_t imageIndex, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
							   vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
							   vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
							   vk::ImageAspectFlags aspectMask);
	void createDepthResources();
	vk::Format findDepthFormat();
	void createSwapChainImageViews();

	// Accessors
	float getAspectRatio() const;
	bool isFrameInProgress() const { return _isFrameStarted; }
	const vk::raii::CommandBuffer& getCurrentCommandBuffer() const { return _frames[_currentFrameIndex].commandBuffer; }
	int getFrameIndex() const { return _currentFrameIndex; }
	uint32_t getCurrentImageIndex() const { return _currentImageIndex; }

	// Resource accessors for Application rendering
	const GraphicsPipeline& getPipeline() const { return *_graphicsPipeline; }
	const vk::raii::DescriptorSet& getDescriptorSet(int frame) const { return _frames[frame].descriptorSet; }
	const vk::raii::Buffer& getVertexBuffer() const { return _vertexBuffer->getBuffer(); }
	const vk::raii::Buffer& getIndexBuffer() const { return _indexBuffer->getBuffer(); }
	const std::vector<vk::raii::ImageView>& getSwapChainImageViews() const { return _swapChainImageViews; }
	const vk::raii::ImageView& getDepthImageView() const { return _depthImageView; }
	vk::Extent2D getSwapChainExtent() const { return _swapChain->getExtent(); }
	vk::Format getSwapChainFormat() const { return _swapChain->getFormat(); }
	uint32_t getIndexCount() const { return static_cast<uint32_t>(_indices.size()); }

private:
	void createCommandBuffers();
	void createSyncObjects();
	void freeCommandBuffers();
	void recreateSwapChain();

	// New initialization methods
	void createGraphicsPipeline();
	void createVertexBuffer();
	void createIndexBuffer();
	void createUniformBuffers();
	void createDescriptorSetLayout();
	void createDescriptorPool();
	void createDescriptorSets();

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

	std::unique_ptr<SwapChain> _swapChain;

	vk::raii::Image _depthImage = nullptr;
	vk::raii::DeviceMemory _depthImageMemory = nullptr;
	vk::raii::ImageView _depthImageView = nullptr;

	vk::Format _depthFormat = vk::Format::eUndefined;

	std::vector<vk::raii::ImageView> _swapChainImageViews;

	std::vector<vk::raii::Semaphore> _renderFinishedSemaphores;

	// Command Buffers
	vk::raii::CommandPool _commandPool = nullptr;

	// Graphics Pipeline
	std::unique_ptr<GraphicsPipeline> _graphicsPipeline;
	vk::raii::DescriptorSetLayout _descriptorSetLayout = nullptr;

	// Buffers
	std::unique_ptr<VulkanBuffer> _vertexBuffer;
	std::unique_ptr<VulkanBuffer> _indexBuffer;

	// Descriptors
	vk::raii::DescriptorPool _descriptorPool = nullptr;

	// Frame Data (Must be declared after pools to ensure correct destruction order)
	std::vector<FrameData> _frames;

	// Geometry data
	const std::vector<Vertex> _vertices = {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
										   {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
										   {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
										   {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

										   {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
										   {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
										   {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
										   {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}};
	const std::vector<uint16_t> _indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

	uint32_t _currentImageIndex = 0;
	int _currentFrameIndex = 0;
	bool _isFrameStarted = false;

	std::unique_ptr<Texture> _texture;
};
} // namespace Fishy