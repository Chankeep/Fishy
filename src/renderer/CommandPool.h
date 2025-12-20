#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS // Disable constructors for Vulkan.hpp

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

namespace Fishy {

class VulkanDevice;

class CommandPool {
public:
	CommandPool(const VulkanDevice& device, uint32_t queueFamilyIndex);
	~CommandPool();

	vk::raii::CommandBuffer allocateBuffer(bool isPrimary = true);

	vk::raii::CommandPool& getCommandPool() { return _pool; }

private:
	const VulkanDevice& _device;
	vk::raii::CommandPool _pool = nullptr;
};
} // namespace Fishy