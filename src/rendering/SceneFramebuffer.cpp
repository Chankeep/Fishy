#include "SceneFramebuffer.h"

#include "core/VulkanDevice.h"

#include <imgui_impl_vulkan.h>

namespace Fishy {

SceneFramebuffer::SceneFramebuffer(VulkanDevice& device, vk::Format colorFormat, uint32_t width, uint32_t height)
	: _device(device),
	  _colorTexture(device, width, height, colorFormat,
					vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled),
	  _depthTexture(device, width, height, vk::Format::eD32Sfloat,
					vk::ImageUsageFlagBits::eDepthStencilAttachment) {
	createSampler();
	registerImGuiTexture();
}

SceneFramebuffer::~SceneFramebuffer() {
	_device->waitIdle();
	unregisterImGuiTexture();
}

void SceneFramebuffer::resize(uint32_t width, uint32_t height) {
	if (!needsResize(width, height)) {
		return;
	}

	_device->waitIdle();

	unregisterImGuiTexture();

	_colorTexture.recreate(width, height);
	_depthTexture.recreate(width, height);

	registerImGuiTexture();

	FISHY_LOG_TRACE("SceneFramebuffer resized to {}x{}", width, height);
}

bool SceneFramebuffer::needsResize(uint32_t w, uint32_t h) const {
	auto extent = _colorTexture.getExtent();
	return extent.width != w || extent.height != h;
}

void SceneFramebuffer::createSampler() {
	vk::SamplerCreateInfo samplerInfo{
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = vk::SamplerAddressMode::eClampToEdge,
		.addressModeV = vk::SamplerAddressMode::eClampToEdge,
		.addressModeW = vk::SamplerAddressMode::eClampToEdge,
		.mipLodBias = 0.0f,
		.anisotropyEnable = vk::False,
		.maxAnisotropy = 1.0f,
		.compareEnable = vk::False,
		.compareOp = vk::CompareOp::eAlways,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = vk::BorderColor::eFloatOpaqueBlack,
		.unnormalizedCoordinates = vk::False,
	};

	_sampler = vk::raii::Sampler(*_device, samplerInfo);
}

void SceneFramebuffer::registerImGuiTexture() {
	auto colorView = _colorTexture.getImageView();
	if (!colorView) {
		return;
	}

	_imguiDescriptor =
		ImGui_ImplVulkan_AddTexture(static_cast<VkSampler>(*_sampler), static_cast<VkImageView>(colorView),
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	FISHY_LOG_TRACE("Registered SceneFramebuffer color texture with ImGui");
}

void SceneFramebuffer::unregisterImGuiTexture() {
	if (_imguiDescriptor != VK_NULL_HANDLE) {
		ImGui_ImplVulkan_RemoveTexture(_imguiDescriptor);
		_imguiDescriptor = VK_NULL_HANDLE;
		FISHY_LOG_TRACE("Unregistered SceneFramebuffer texture from ImGui");
	}
}

} // namespace Fishy