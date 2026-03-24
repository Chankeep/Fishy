#pragma once

#include "VulkanDevice.h"

namespace Fishy {

class VulkanImage {
public:
	// Default constructor (used for container placeholder)
	VulkanImage() = default;

	// Core constructor
	// allocator: VMA allocator
	// imageInfo: Vulkan image creation info (using C++ bindings)
	// allocInfo: VMA memory allocation strategy
	VulkanImage(VmaAllocator allocator, const vk::ImageCreateInfo& imageInfo, const VmaAllocationCreateInfo& allocInfo);

	~VulkanImage();

	VulkanImage(const VulkanImage&) = delete;
	VulkanImage& operator=(const VulkanImage&) = delete;

	VulkanImage(VulkanImage&& other) noexcept;
	VulkanImage& operator=(VulkanImage&& other) noexcept;

	operator vk::Image() const { return _image; }
	[[nodiscard]] vk::Image getImage() const { return _image; }
	[[nodiscard]] VmaAllocation getAllocation() const { return _allocation; }

	// Create and manage the primary view (lifecycle bound to this Image)
	// Note: This will overwrite the internally stored view
	void createView(const vk::raii::Device& device, const vk::ImageViewCreateInfo& viewInfo);

	// Get the managed View (if created)
	[[nodiscard]] const vk::raii::ImageView& getView() const { return _view; }

private:
	void destroy(); // Internal cleanup helper function

	VmaAllocator _allocator = nullptr;

	// ⚠️ Critical order: Image must be destructed after View.
	// In C++, members are constructed in declaration order and destructed in reverse order.
	// So View (declared last) will be destructed first, and Image (declared first) afterwards.
	VkImage _image = VK_NULL_HANDLE;
	VmaAllocation _allocation = nullptr;

	// Optional: Internally held View, ensuring it dies before Image
	vk::raii::ImageView _view = nullptr;
};

} // namespace Fishy