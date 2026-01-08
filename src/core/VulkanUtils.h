#pragma once

#include "VulkanDevice.h"

namespace Fishy {

/**
 * @brief Utility functions for Vulkan operations
 */
namespace VulkanUtils {

#ifndef NDEBUG
/**
 * @brief Set the debug name for a Vulkan object (VK_EXT_debug_utils)
 */
template <typename T>
inline void setDebugName(const VulkanDevice& device, T handle, vk::ObjectType type, const char* name) {
	if (!name)
		return;
	VkDebugUtilsObjectNameInfoEXT nameInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
	nameInfo.objectType = static_cast<VkObjectType>(type);
	nameInfo.objectHandle = reinterpret_cast<uint64_t>(static_cast<typename T::NativeType>(handle));
	nameInfo.pObjectName = name;

	vkSetDebugUtilsObjectNameEXT(*(*device), &nameInfo);
}

// Helper overload for raw VkImage
inline void setDebugName(const VulkanDevice& device, VkImage handle, const char* name) {
	if (!name)
		return;
	VkDebugUtilsObjectNameInfoEXT nameInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
	nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
	nameInfo.objectHandle = reinterpret_cast<uint64_t>(handle);
	nameInfo.pObjectName = name;

	vkSetDebugUtilsObjectNameEXT(*(*device), &nameInfo);
}

// Helper overload for raw VkBuffer
inline void setDebugName(const VulkanDevice& device, VkBuffer handle, const char* name) {
	if (!name)
		return;
	VkDebugUtilsObjectNameInfoEXT nameInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
	nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
	nameInfo.objectHandle = reinterpret_cast<uint64_t>(handle);
	nameInfo.pObjectName = name;

	vkSetDebugUtilsObjectNameEXT(*(*device), &nameInfo);
}
#else
// No-op in release
template <typename T> inline void setDebugName(const VulkanDevice&, T, vk::ObjectType, const char*) {}
inline void setDebugName(const VulkanDevice&, VkImage, const char*) {}
inline void setDebugName(const VulkanDevice&, VkBuffer, const char*) {}
#endif

/**
 * @brief Execute commands immediately using a one-time command buffer
 *
 * Allocates a command buffer, executes the provided commands, submits,
 * and waits for completion. Useful for resource initialization.
 *
 * @tparam Func Lambda or callable that takes vk::raii::CommandBuffer&
 * @param device VulkanDevice to use for command pool and queue
 * @param commands Lambda containing commands to execute
 *
 * @example
 * VulkanUtils::executeImmediate(device, [&](auto& cmd) {
 *     transitionImage(cmd, image, ...);
 *     cmd.copyBufferToImage(...);
 * });
 */
template <typename Func> void executeImmediate(const VulkanDevice& device, Func&& commands) {
	auto cmd = device.getTransferCommandPool().allocateBuffer(true);

	cmd.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

	commands(cmd);

	cmd.end();

	vk::SubmitInfo submitInfo{
		.commandBufferCount = 1,
		.pCommandBuffers = &(*cmd),
	};

	device.getGraphicsQueue().submit(submitInfo, nullptr);
	device.getGraphicsQueue().waitIdle();
}

/**
 * @brief Transition an image between layouts using pipeline barriers
 *
 * @param cmd Command buffer to record the barrier
 * @param image Image to transition
 * @param oldLayout Current layout of the image
 * @param newLayout Target layout
 * @param srcAccessMask Source access mask
 * @param dstAccessMask Destination access mask
 * @param srcStageMask Source pipeline stage
 * @param dstStageMask Destination pipeline stage
 * @param aspectMask Image aspect (color, depth, stencil)
 */
inline void transitionImage(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout oldLayout,
							vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
							vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
							vk::ImageAspectFlags aspectMask) {
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

/**
 * @brief Transition an image between layouts with custom mip levels and array layers
 *
 * @param cmd Command buffer to record the barrier
 * @param image Image to transition
 * @param oldLayout Current layout of the image
 * @param newLayout Target layout
 * @param srcAccessMask Source access mask
 * @param dstAccessMask Destination access mask
 * @param srcStageMask Source pipeline stage
 * @param dstStageMask Destination pipeline stage
 * @param aspectMask Image aspect (color, depth, stencil)
 * @param mipLevels Number of mip levels to transition
 * @param layerCount Number of array layers to transition
 */
inline void transitionImage(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout oldLayout,
							vk::ImageLayout newLayout, vk::AccessFlags2 srcAccessMask, vk::AccessFlags2 dstAccessMask,
							vk::PipelineStageFlags2 srcStageMask, vk::PipelineStageFlags2 dstStageMask,
							vk::ImageAspectFlags aspectMask, uint32_t mipLevels, uint32_t layerCount) {
	vk::ImageMemoryBarrier2 barrier{.srcStageMask = srcStageMask,
									.srcAccessMask = srcAccessMask,
									.dstStageMask = dstStageMask,
									.dstAccessMask = dstAccessMask,
									.oldLayout = oldLayout,
									.newLayout = newLayout,
									.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
									.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
									.image = image,
									.subresourceRange = {.aspectMask = aspectMask,
														 .baseMipLevel = 0,
														 .levelCount = mipLevels,
														 .baseArrayLayer = 0,
														 .layerCount = layerCount}};

	vk::DependencyInfo dependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier};

	cmd.pipelineBarrier2(dependencyInfo);
}

} // namespace VulkanUtils

} // namespace Fishy
