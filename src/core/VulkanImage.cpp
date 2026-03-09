#include "VulkanImage.h"

namespace Fishy {

VulkanImage::VulkanImage(VmaAllocator allocator, const vk::ImageCreateInfo& imageInfo,
						 const VmaAllocationCreateInfo& allocInfo)
	: _allocator(allocator) {

	// vulkan.hpp 的结构体二进制兼容 Vk 结构体，可以直接 reinterpret_cast
	const VkImageCreateInfo& rawInfo = static_cast<const VkImageCreateInfo&>(imageInfo);

	VkResult result = vmaCreateImage(_allocator, &rawInfo, &allocInfo, &_image, &_allocation, nullptr);

	if (result != VK_SUCCESS) {
		FISHY_LOG_ERROR("Failed to create VMA depth image: {}x{}", imageInfo.extent.width,
							   imageInfo.extent.height);
		throw std::runtime_error("Failed to create VMA Image!");
	}
}

VulkanImage::~VulkanImage() { destroy(); }

// 移动构造
VulkanImage::VulkanImage(VulkanImage&& other) noexcept
	: _allocator(other._allocator), _image(other._image), _allocation(other._allocation),
	  _view(std::move(other._view)) { // View 也需要移动

	// 重置源对象，防止 Double Free
	other._image = VK_NULL_HANDLE;
	other._allocation = nullptr;
	other._allocator = nullptr;
}

// 移动赋值
VulkanImage& VulkanImage::operator=(VulkanImage&& other) noexcept {
	if (this != &other) {
		destroy(); // 先清理自己的旧资源

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
	// 1. 先销毁 View (vk::raii 成员变量会自动处理，但如果有手动逻辑写在这里)
	_view.clear(); // 显式清除 View，尽管析构函数会自动做

	// 2. 再销毁 Image
	if (_image && _allocator) {
		vmaDestroyImage(_allocator, _image, _allocation);
		_image = VK_NULL_HANDLE;
		_allocation = nullptr;
	}
}

void VulkanImage::createView(const vk::raii::Device& device, const vk::ImageViewCreateInfo& viewInfo) {
	// 确保 viewInfo 中的 image 句柄指向自己
	vk::ImageViewCreateInfo info = viewInfo;
	info.image = _image;

	// 赋值给成员变量，利用 RAII 接管生命周期
	_view = vk::raii::ImageView(device, info);
}

} // namespace Fishy