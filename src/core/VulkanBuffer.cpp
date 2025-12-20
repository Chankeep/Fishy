#include "VulkanBuffer.h"
#include <iostream>

namespace Fishy {

VulkanBuffer::VulkanBuffer(const VulkanDevice& device, vk::DeviceSize size, vk::BufferUsageFlags usage,
						   vk::MemoryPropertyFlags properties)
	: _device(&device), _size(size) {

	// 1. Create Buffer
	vk::BufferCreateInfo bufferInfo{.size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};

	_buffer = vk::raii::Buffer(**_device, bufferInfo);

	// 2. Get Memory Requirements
	vk::MemoryRequirements memRequirements = _buffer.getMemoryRequirements();

	// 3. Allocate Memory
	vk::MemoryAllocateInfo allocInfo{.allocationSize = memRequirements.size,
									 .memoryTypeIndex =
										 _device->findMemoryType(memRequirements.memoryTypeBits, properties)};

	try {
		_memory = vk::raii::DeviceMemory(**_device, allocInfo);
	} catch (const std::exception& e) {
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	// 4. Bind Memory
	_buffer.bindMemory(*_memory, 0);
}

VulkanBuffer::~VulkanBuffer() {
	// RAII handles destruction
}

void* VulkanBuffer::map(vk::DeviceSize offset, vk::DeviceSize size) {
	if (!_mapped) {
		_mapped = _memory.mapMemory(offset, size);
	}
	return _mapped;
}

void VulkanBuffer::unmap() {
	if (_mapped) {
		_memory.unmapMemory();
		_mapped = nullptr;
	}
}

bool VulkanBuffer::upload(void* data, vk::DeviceSize size) {
	void* mappedData = map(0, size);
	if (mappedData) {
		memcpy(mappedData, data, static_cast<size_t>(size));
		// If memory is not Coherent, we might need flush here, but assuming Coherent for simplicity for map/write
		// usually. Or we should check properties.
		unmap();
		return true;
	}
	return false;
}

void VulkanBuffer::uploadStaged(const vk::raii::CommandPool& commandPool, const vk::raii::Queue& queue, void* data,
								vk::DeviceSize size) {

	// 1. Create Staging Buffer (Host Visible | Coherent)
	VulkanBuffer stagingBuffer(*_device, size, vk::BufferUsageFlagBits::eTransferSrc,
							   vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	// 2. Map and Copy
	stagingBuffer.upload(data, size);

	// 3. Allocate Temporary Command Buffer
	vk::CommandBufferAllocateInfo allocInfo{
		.commandPool = *commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};

	vk::raii::CommandBuffers cmdbuffers(**_device, allocInfo);
	vk::raii::CommandBuffer& cmd = cmdbuffers[0];

	// 4. Record Copy Command
	vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
	cmd.begin(beginInfo);

	copyBuffer(*_device, cmd, stagingBuffer, *this, size);

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
	cmd.copyBuffer(*srcBuffer.getBuffer(), *dstBuffer.getBuffer(), copyRegion);
}

} // namespace Fishy
