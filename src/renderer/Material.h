#pragma once

#include "Texture.h"
#include <memory>
#include <string>

namespace Fishy {

class VulkanDevice;

class Material {
public:
	// Pure data holder, no loading responsibility
	struct Config {
		std::shared_ptr<Texture> albedo;
		std::shared_ptr<Texture> metallic;
		std::shared_ptr<Texture> roughness;
		std::shared_ptr<Texture> normal;
		std::shared_ptr<Texture> ao;
		std::shared_ptr<Texture> height;
	};

	Material(const VulkanDevice& device, const Config& config);
	~Material();

	// Create descriptor set for this material.
	// Assumes layout has bindings:
	// 0: Albedo
	// 1: Metallic
	// 2: Roughness
	// 3: Normal
	void createDescriptorSet(vk::DescriptorPool descriptorPool, vk::DescriptorSetLayout layout);

	const vk::raii::DescriptorSet& getDescriptorSet() const { return _descriptorSet; }

	// Accessors
	const std::shared_ptr<Texture>& getAlbedoMap() const { return _albedoMap; }
	const std::shared_ptr<Texture>& getMetallicMap() const { return _metallicMap; }
	const std::shared_ptr<Texture>& getRoughnessMap() const { return _roughnessMap; }
	const std::shared_ptr<Texture>& getNormalMap() const { return _normalMap; }
	const std::shared_ptr<Texture>& getAoMap() const { return _aoMap; }
	const std::shared_ptr<Texture>& getHeightMap() const { return _heightMap; }

private:
	const VulkanDevice& _device;

	std::shared_ptr<Texture> _albedoMap;
	std::shared_ptr<Texture> _metallicMap;
	std::shared_ptr<Texture> _roughnessMap;
	std::shared_ptr<Texture> _normalMap;
	std::shared_ptr<Texture> _aoMap;
	std::shared_ptr<Texture> _heightMap;

	vk::raii::DescriptorSet _descriptorSet = nullptr;
};

} // namespace Fishy
