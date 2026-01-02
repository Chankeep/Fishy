#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include "VulkanDevice.h"

namespace Fishy {

/**
 * @brief Utility functions for Vulkan operations
 */
namespace VulkanUtils {

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
