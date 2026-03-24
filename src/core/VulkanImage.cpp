#include "VulkanImage.h"

namespace Fishy {

VulkanImage::VulkanImage(VmaAllocator allocator, const vk::ImageCreateInfo& imageInfo,
						 const VmaAllocationCreateInfo& allocInfo)
	: _allocator(allocator) {

	// vulkan.hpp structs are binary compatible with Vk structs, can be directly casted
	const VkImageCreateInfo& rawInfo = static_cast<const VkImageCreateInfo&>(imageInfo);

	VkResult result = vmaCreateImage(_allocator, &rawInfo, &allocInfo, &_image, &_allocation, nullptr);

	if (result != VK_SUCCESS) {
		FISHY_LOG_ERROR("Failed to create VMA depth image: {}x{}", imageInfo.extent.width,
							   imageInfo.extent.height);
		throw std::runtime_error("Failed to create VMA Image!");
	}
}

VulkanImage::~VulkanImage() { destroy(); }

// Move constructor
VulkanImage::VulkanImage(VulkanImage&& other) noexcept
	: _allocator(other._allocator), _image(other._image), _allocation(other._allocation),
	  _view(std::move(other._view)) { // View also needs to be moved

	// Reset source object to prevent Double Free
	other._image = VK_NULL_HANDLE;
	other._allocation = nullptr;
	other._allocator = nullptr;
}

// Move assignment
VulkanImage& VulkanImage::operator=(VulkanImage&& other) noexcept {
	if (this != &other) {
		destroy(); // Clean up own old resources first

		_allocator = other._allocator;
		_image = other._image;
		_allocation = other._allocation;
		_view = std::move(other._view);

		other._image = VK_NULL_HANDLE;
		other._allocation = nullptr;
		other._allocator = nullptr;
	}
	return *this;
}

void VulkanImage::destroy() {
	// 1. Destroy View first (vk::raii member automatically handles it, but manual logic can go here)
	_view.clear(); // Explicitly clear View, even though destructor handles it

	// 2. Destroy Image next
	if (_image && _allocator) {
		vmaDestroyImage(_allocator, _image, _allocation);
		_image = VK_NULL_HANDLE;
		_allocation = nullptr;
	}
}

void VulkanImage::createView(const vk::raii::Device& device, const vk::ImageViewCreateInfo& viewInfo) {
	// Ensure image handle in viewInfo points to self
	vk::ImageViewCreateInfo info = viewInfo;
	info.image = _image;

	// Assign to member variable, using RAII to govern lifecycle
	_view = vk::raii::ImageView(device, info);
}

} // namespace Fishy