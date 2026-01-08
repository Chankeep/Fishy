#include "VulkanBuffer.h"
#include "CommandPool.h"
#include "LogSystem.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"
#include <format>
#include <iostream>

namespace Fishy {

static std::string getBufferUsageDescription(vk::BufferUsageFlags usage) {
	if (usage == vk::BufferUsageFlagBits::eTransferSrc) {
		return "staging";
	}
	std::vector<std::string_view> usages;
	if (usage & vk::BufferUsageFlagBits::eVertexBuffer)
		usages.emplace_back("vertex");
	if (usage & vk::BufferUsageFlagBits::eIndexBuffer)
		usages.emplace_back("index");
	if (usage & vk::BufferUsageFlagBits::eUniformBuffer)
		usages.emplace_back("uniform");
	if (usage & vk::BufferUsageFlagBits::eStorageBuffer)
		usages.emplace_back("storage");
	if (usage & vk::BufferUsageFlagBits::eTransferDst)
		usages.emplace_back("transfer-dst");
	if (usage & vk::BufferUsageFlagBits::eIndirectBuffer)
		usages.emplace_back("indirect");
	if (usages.empty())
		return "unknown";
	// Future: C++23 std::format("{}", std::views::join_with(usages, " + "))
	// C++20 workaround:
	std::string result;
	for (size_t i = 0; i < usages.size(); ++i) {
		if (i > 0)
			result += " + ";
		result += usages[i];
	}
	return result;
}

VulkanBuffer::VulkanBuffer(const VulkanDevice& device, vk::DeviceSize size, vk::BufferUsageFlags usage,
						   vk::MemoryPropertyFlags properties, const char* debugName)
	: _device(device), _size(size) {

#ifndef NDEBUG
	if (debugName)
		_debugName = debugName;
#endif

	// 1. Create Buffer using VMA
	VkBufferCreateInfo bufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = static_cast<VkBufferUsageFlags>(usage),
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo allocInfo = {.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
												  VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
												  VMA_ALLOCATION_CREATE_MAPPED_BIT,
										 .usage = VMA_MEMORY_USAGE_AUTO};

	// Determine memory strategy based on requested properties
	// if (properties & vk::MemoryPropertyFlagBits::eHostVisible) {
	// 	// Strategy: Persistent mapping for frequently updated data (Uniform Buffers)
	// 	allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	// 	allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	// 	allocInfo.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	// } else {
	// 	// Strategy: Device local for static data (Vertex/Index Buffers)
	// 	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	// }

	VmaAllocationInfo allocInfoOut;

	VkResult result =
		vmaCreateBuffer(_device.getVmaAllocator(), &bufferInfo, &allocInfo, &_buffer, &_vmaAllocation, &allocInfoOut);

	if (result != VK_SUCCESS) {
		LogSystem::get().error("Failed to create VMA buffer: {} bytes", size);
		throw std::runtime_error("Failed to create VMA buffer!");
	}

	// VMA already filled _buffer, store mapped pointer if available
	_mappedData = allocInfoOut.pMappedData;

	std::string usageDesc = getBufferUsageDescription(usage);
	LogSystem::get().trace("Created {} buffer: {} bytes ({:.2f} MB)", usageDesc, size, size / (1024.0 * 1024.0));

#ifndef NDEBUG
	if (!_debugName.empty()) {
		VulkanUtils::setDebugName(_device, _buffer, _debugName.c_str());
	}
#endif
}

VulkanBuffer::~VulkanBuffer() {
	if (_vmaAllocation && _device.getVmaAllocator()) {
		vmaDestroyBuffer(_device.getVmaAllocator(), _buffer, _vmaAllocation);
	}
}

void* VulkanBuffer::map(vk::DeviceSize offset, vk::DeviceSize size) {
	if (_mappedData) {
		// Already mapped persistently, return offset pointer
		return static_cast<char*>(_mappedData) + offset;
	}

	// Fallback: map on-demand (should not happen with VMA_MAPPING strategy)
	void* data = nullptr;
	VkResult result = vmaMapMemory(_device.getVmaAllocator(), _vmaAllocation, &data);
	if (result == VK_SUCCESS) {
		return static_cast<char*>(data) + offset;
	}
	return nullptr;
}

void VulkanBuffer::unmap() {
	if (!_mappedData) {
		// Only unmap if not persistently mapped
		vmaUnmapMemory(_device.getVmaAllocator(), _vmaAllocation);
	}
}

void VulkanBuffer::upload(std::span<const std::byte> data) {
	void* mappedData = map(0, data.size());
	if (mappedData) {
		memcpy(mappedData, data.data(), data.size());
		unmap();
		return;
	}
	LogSystem::get().error("Failed to upload {} bytes to buffer", data.size());
	throw std::runtime_error("Failed to upload data to buffer!");
}

void VulkanBuffer::uploadStaged(CommandPool& commandPool, const vk::raii::Queue& queue,
								std::span<const std::byte> data) {
	// 1. Create Staging Buffer (Host Visible | Coherent with persistent mapping)
	VulkanBuffer stagingBuffer(_device, data.size(), vk::BufferUsageFlagBits::eTransferSrc,
							   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	// 2. Map and Copy
	stagingBuffer.upload(data);

	// 3. Execute copy using one-time command buffer
	VulkanUtils::executeImmediate(_device, [&](auto& cmd) { copyBuffer(cmd, stagingBuffer, *this, data.size()); });

	// stagingBuffer is destroyed here (RAII)
}

void VulkanBuffer::copyBuffer(const vk::raii::CommandBuffer& cmd, const VulkanBuffer& srcBuffer,
							  VulkanBuffer& dstBuffer, vk::DeviceSize size) {
	vk::BufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	cmd.copyBuffer(srcBuffer.getHandle(), dstBuffer.getHandle(), copyRegion);
}

uint64_t VulkanBuffer::getDeviceAddress() const {
	vk::BufferDeviceAddressInfo bufferAddressInfo{};
	bufferAddressInfo.buffer = _buffer;
	return _device->getBufferAddress(bufferAddressInfo);
}

} // namespace Fishy
