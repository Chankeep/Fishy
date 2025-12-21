#include "Material.h"
#include "../core/VulkanDevice.h"
#include <iostream>
#include <vector>

namespace Fishy {

Material::Material(const VulkanDevice& device, const Config& config) : _device(device) {
	_albedoMap = config.albedo;
	_metallicMap = config.metallic;
	_roughnessMap = config.roughness;
	_normalMap = config.normal;
	_aoMap = config.ao;
	_heightMap = config.height;
}

Material::~Material() {}

void Material::createDescriptorSet(vk::DescriptorPool descriptorPool, vk::DescriptorSetLayout layout) {
	vk::DescriptorSetAllocateInfo allocInfo{
		.descriptorPool = descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	auto sets = vk::raii::DescriptorSets(*_device, allocInfo);
	_descriptorSet = std::move(sets[0]);

	std::vector<vk::WriteDescriptorSet> descriptorWrites;
	std::vector<vk::DescriptorImageInfo> imageInfos;
	// We might have up to 6 textures (Albedo, Metallic, Roughness, Normal, AO, Height)
	imageInfos.reserve(6);

	auto addTextureWrite = [&](const std::shared_ptr<Texture>& texture, uint32_t binding) {
		if (texture) {
			auto& info = imageInfos.emplace_back(vk::DescriptorImageInfo{
				.sampler = *texture->getSampler(),
				.imageView = *texture->getImageView(),
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			});

			descriptorWrites.push_back(vk::WriteDescriptorSet{
				.dstSet = *_descriptorSet,
				.dstBinding = binding,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.pImageInfo = &info,
			});
		}
	};

	// Fallback to albedo if specific maps are missing to ensure descriptors are valid
	auto safeAlbedo = _albedoMap;
	if (!safeAlbedo) {
		// This is critical, but we assume albedo is provided for now or we won't draw anything useful.
		// Ideally we should have a 1x1 pink default texture.
		std::cerr << "Warning: Material has no albedo map!" << std::endl;
		return;
	}

	addTextureWrite(safeAlbedo, 0);
	addTextureWrite(_metallicMap ? _metallicMap : safeAlbedo, 1);
	addTextureWrite(_roughnessMap ? _roughnessMap : safeAlbedo, 2);
	addTextureWrite(_normalMap ? _normalMap : safeAlbedo, 3);
	addTextureWrite(_aoMap ? _aoMap : safeAlbedo, 4);
	addTextureWrite(_heightMap ? _heightMap : safeAlbedo, 5);

	if (!descriptorWrites.empty()) {
		_device->updateDescriptorSets(descriptorWrites, nullptr);
	}
}

} // namespace Fishy
