#pragma once

#include "VulkanDevice.h"
#include <cstdint>
#include <span>

namespace Fishy {

class VulkanBuffer {
public:
	VulkanBuffer(const VulkanDevice& device, vk::DeviceSize size, vk::BufferUsageFlags usage,
				 vk::MemoryPropertyFlags properties);
	~VulkanBuffer();

	VulkanBuffer(const VulkanBuffer&) = delete;
	VulkanBuffer& operator=(const VulkanBuffer&) = delete;
	VulkanBuffer(VulkanBuffer&&) = delete;
	VulkanBuffer& operator=(VulkanBuffer&&) = delete;

	[[nodiscard]] VkBuffer getHandle() const { return _buffer; }
	[[nodiscard]] vk::Buffer getBuffer() const { return vk::Buffer(_buffer); }
	[[nodiscard]] vk::DeviceSize getSize() const { return _size; }
	[[nodiscard]] void* getMappedPtr() const { return _mappedData; }
	[[nodiscard]] uint64_t getDeviceAddress() const;
	// Maps the memory to a host pointer.
	// Use for HostVisible memory.
	void* map(vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE);
	void unmap();

	// Copies data to the mapped memory.
	// memory must be HostVisible.
	void upload(std::span<const std::byte> data);
	void upload(const void* data, size_t size) { upload({static_cast<const std::byte*>(data), size}); }

	// Uploads data using a temporary staging buffer.
	// Executes the copy command immediately and waits for completion.
	// Requires a command pool to allocate a temporary command buffer.
	void uploadStaged(class CommandPool& commandPool, const vk::raii::Queue& queue, std::span<const std::byte> data);

	// Static helper to copy buffer content using a command buffer.
	// The command buffer must be in recording state.
	static void copyBuffer(const vk::raii::CommandBuffer& cmd, // Not saving this instance, just using it
						   const VulkanBuffer& srcBuffer, VulkanBuffer& dstBuffer, vk::DeviceSize size);

private:
	const VulkanDevice& _device;
	vk::DeviceSize _size;
	VkBuffer _buffer = VK_NULL_HANDLE;
	VmaAllocation _vmaAllocation = nullptr;
	void* _mappedData = nullptr;
};

} // namespace Fishy
