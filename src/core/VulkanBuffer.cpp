#include "VulkanBuffer.h"
#include "CommandPool.h"
#include <iostream>

namespace Fishy {

static std::string getBufferUsageDescription(vk::BufferUsageFlags usage) {
	// TransferSrc alone is staging buffer
	if (usage == vk::BufferUsageFlagBits::eTransferSrc) {
		return "staging";
	}

	std::vector<std::string> usages;

	if (usage & vk::BufferUsageFlagBits::eVertexBuffer)
		usages.push_back("vertex");
	if (usage & vk::BufferUsageFlagBits::eIndexBuffer)
		usages.push_back("index");
	if (usage & vk::BufferUsageFlagBits::eUniformBuffer)
		usages.push_back("uniform");
	if (usage & vk::BufferUsageFlagBits::eStorageBuffer)
		usages.push_back("storage");
	if (usage & vk::BufferUsageFlagBits::eTransferDst)
		usages.push_back("transfer-dst");
	if (usage & vk::BufferUsageFlagBits::eIndirectBuffer)
		usages.push_back("indirect");

	if (usages.empty())
		return "unknown";

	// Combine usage descriptions
	std::string result;
	for (size_t i = 0; i < usages.size(); ++i) {
		if (i > 0)
			result += " + ";
		result += usages[i];
	}
	return result;
}

VulkanBuffer::VulkanBuffer(const VulkanDevice& device, vk::DeviceSize size, vk::BufferUsageFlags usage,
						   vk::MemoryPropertyFlags properties)
	: _device(device), _size(size), _vmaAllocator(device.getVmaAllocator()) {

	// 1. Create Buffer using VMA
	VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = static_cast<VkBufferUsageFlags>(usage),
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo allocInfo = {};

	// Determine memory strategy based on requested properties
	if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
		// Strategy: Persistent mapping for frequently updated data (Uniform Buffers)
		allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
		allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	} else {
		// Strategy: Device local for static data (Vertex/Index Buffers)
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	}

	VmaAllocationInfo allocInfoOut;

	VkResult result = vmaCreateBuffer(_vmaAllocator, &bufferInfo, &allocInfo, &_buffer, &_vmaAllocation, &allocInfoOut);

	if (result != VK_SUCCESS) {
		LogSystem::get().error("Failed to create VMA buffer: {} bytes", size);
		throw std::runtime_error("Failed to create VMA buffer!");
	}

	// VMA already filled _buffer, store mapped pointer if available
	_mappedData = allocInfoOut.pMappedData;

	std::string usageDesc = getBufferUsageDescription(usage);
	LogSystem::get().trace("Created {} buffer: {} bytes ({:.2f} MB)", usageDesc, size, size / (1024.0 * 1024.0));
}

VulkanBuffer::~VulkanBuffer() {
	if (_vmaAllocation && _vmaAllocator) {
		vmaDestroyBuffer(_vmaAllocator, _buffer, _vmaAllocation);
	}
}

void* VulkanBuffer::map(vk::DeviceSize offset, vk::DeviceSize size) {
	if (_mappedData) {
		// Already mapped persistently, return offset pointer
		return static_cast<char*>(_mappedData) + offset;
	}

	// Fallback: map on-demand (should not happen with VMA_MAPPING strategy)
	void* data = nullptr;
	VkResult result = vmaMapMemory(_vmaAllocator, _vmaAllocation, &data);
	if (result == VK_SUCCESS) {
		return static_cast<char*>(data) + offset;
	}
	return nullptr;
}

void VulkanBuffer::unmap() {
	if (!_mappedData) {
		// Only unmap if not persistently mapped
		vmaUnmapMemory(_vmaAllocator, _vmaAllocation);
	}
}

bool VulkanBuffer::upload(void* data, vk::DeviceSize size) {
	void* mappedData = map(0, size);
	if (mappedData) {
		memcpy(mappedData, data, static_cast<size_t>(size));
		// VMA with HOST_COHERENT doesn't need explicit flush
		unmap();
		return true;
	}
	return false;
}

void VulkanBuffer::uploadStaged(CommandPool& commandPool, const vk::raii::Queue& queue, void* data,
								vk::DeviceSize size) {

	// 1. Create Staging Buffer (Host Visible | Coherent with persistent mapping)
	VulkanBuffer stagingBuffer(_device, size, vk::BufferUsageFlagBits::eTransferSrc,
							   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	// 2. Map and Copy
	stagingBuffer.upload(data, size);

	// 3. Allocate Temporary Command Buffer
	vk::CommandBufferAllocateInfo allocInfo{.commandPool = *commandPool.getCommandPool(),
											.level = vk::CommandBufferLevel::ePrimary,
											.commandBufferCount = 1};

	vk::raii::CommandBuffers cmdbuffers(*_device, allocInfo);
	vk::raii::CommandBuffer& cmd = cmdbuffers[0];

	// 4. Record Copy Command
	vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
	cmd.begin(beginInfo);

	copyBuffer(_device, cmd, stagingBuffer, *this, size);

	cmd.end();

	// 5. Submit and Wait
	vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &(*cmd)};

	queue.submit(submitInfo, nullptr); // No fence, simply wait idle
	queue.waitIdle();

	// stagingBuffer handles and temporary cmd buffers are destroyed here (RAII)
}

void VulkanBuffer::copyBuffer(const VulkanDevice& device, const vk::raii::CommandBuffer& cmd,
							  const VulkanBuffer& srcBuffer, VulkanBuffer& dstBuffer, vk::DeviceSize size) {
	vk::BufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	cmd.copyBuffer(srcBuffer.getHandle(), dstBuffer.getHandle(), copyRegion);
}

} // namespace Fishy
