#include "VulkanBuffer.h"
#include <iostream>

namespace Fishy {

static std::string getBufferUsageDescription(vk::BufferUsageFlags usage) {
	// TransferSrc alone is staging buffer
	if (usage == vk::BufferUsageFlagBits::eTransferSrc) {
		return "staging";
	}

	std::vector<std::string> usages;

	if (usage & vk::BufferUsageFlagBits::eVertexBuffer) usages.push_back("vertex");
	if (usage & vk::BufferUsageFlagBits::eIndexBuffer) usages.push_back("index");
	if (usage & vk::BufferUsageFlagBits::eUniformBuffer) usages.push_back("uniform");
	if (usage & vk::BufferUsageFlagBits::eStorageBuffer) usages.push_back("storage");
	if (usage & vk::BufferUsageFlagBits::eTransferDst) usages.push_back("transfer-dst");
	if (usage & vk::BufferUsageFlagBits::eIndirectBuffer) usages.push_back("indirect");

	if (usages.empty()) return "unknown";

	// Combine usage descriptions
	std::string result;
	for (size_t i = 0; i < usages.size(); ++i) {
		if (i > 0) result += " + ";
		result += usages[i];
	}
	return result;
}

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
		LogSystem::get().error("Failed to allocate buffer memory: {} bytes", size);
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	std::string usageDesc = getBufferUsageDescription(usage);
	LogSystem::get().trace("Created {} buffer: {} bytes ({:.2f} MB)", usageDesc, size, size / (1024.0 * 1024.0));

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
