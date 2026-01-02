#include "CommandPool.h"
#include "VulkanDevice.h"

namespace Fishy {

CommandPool::CommandPool(const VulkanDevice& device, uint32_t queueFamilyIndex) : _device(device) {
	vk::CommandPoolCreateInfo poolInfo{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
									   .queueFamilyIndex = queueFamilyIndex};
	_pool = vk::raii::CommandPool(*_device, poolInfo);
}

vk::raii::CommandBuffer CommandPool::allocateBuffer(bool isPrimary) {
	vk::CommandBufferAllocateInfo allocInfo{.commandPool = *_pool,
											.level = isPrimary ? vk::CommandBufferLevel::ePrimary
															   : vk::CommandBufferLevel::eSecondary,
											.commandBufferCount = 1};
	vk::raii::CommandBuffer commandBuffer = std::move(_device->allocateCommandBuffers(allocInfo).front());

	return commandBuffer;
}

} // namespace Fishy