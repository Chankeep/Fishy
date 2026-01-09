#pragma once

#include "VulkanDevice.h"

namespace Fishy {

class VulkanImage {
public:
	// 默认构造函数 (用于容器占位)
	VulkanImage() = default;

	// 核心构造函数
	// allocator: VMA 分配器
	// imageInfo: Vulkan 图片创建信息 (使用 C++ 绑定类型)
	// allocInfo: VMA 内存分配策略
	VulkanImage(VmaAllocator allocator, const vk::ImageCreateInfo& imageInfo, const VmaAllocationCreateInfo& allocInfo);

	~VulkanImage();

	VulkanImage(const VulkanImage&) = delete;
	VulkanImage& operator=(const VulkanImage&) = delete;

	VulkanImage(VulkanImage&& other) noexcept;
	VulkanImage& operator=(VulkanImage&& other) noexcept;

	operator vk::Image() const { return _image; }
	[[nodiscard]] vk::Image getImage() const { return _image; }
	[[nodiscard]] VmaAllocation getAllocation() const { return _allocation; }

	// 创建并管理主视图 (生命周期绑定到此 Image)
	// 注意：这将覆盖内部存储的 view
	void createView(const vk::raii::Device& device, const vk::ImageViewCreateInfo& viewInfo);

	// 获取被管理的 View (如果已创建)
	[[nodiscard]] const vk::raii::ImageView& getView() const { return _view; }

private:
	void destroy(); // 内部清理帮助函数

	VmaAllocator _allocator = nullptr;

	// ⚠️ 关键顺序：Image 必须在 View 之后析构。
	// 在 C++ 中，成员按声明顺序构造，按逆序析构。
	// 所以 View (最后声明) 会先被析构，Image (先声明) 后析构。
	VkImage _image = VK_NULL_HANDLE;
	VmaAllocation _allocation = nullptr;

	// 可选：内部持有的 View，确保它比 Image 先死
	vk::raii::ImageView _view = nullptr;
};

} // namespace Fishy