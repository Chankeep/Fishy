#include "ResourceManager.h"
#include "../core/VulkanDevice.h"
#include "../renderer/Material.h"
#include "CubemapTexture.h"
#include "Mesh.h"
#include "Texture.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace Fishy {

ResourceManager::ResourceManager(VulkanDevice& device) : _device(device) {}

ResourceManager::~ResourceManager() { clear(); }

void ResourceManager::initMaterialResources(vk::DescriptorSetLayout materialLayout,
											const vk::raii::DescriptorPool& descriptorPool) {
	LogSystem::get().info("Initializing ResourceManager material resources...");
	_materialLayout = materialLayout;
	_descriptorPool = &descriptorPool;

	// Create default textures
	createDefaultTextures();
}

void ResourceManager::createDefaultTextures() {
	LogSystem::get().info("Creating default textures (white, normal)...");

	// Create 1x1 white texture (RGBA) - sRGB for color data
	unsigned char whitePixel[] = {255, 255, 255, 255};
	_defaultWhiteTexture = std::make_shared<Texture>(_device, whitePixel, 1, 1, vk::Format::eR8G8B8A8Srgb);

	// Create 1x1 default normal map (pointing up: RGB = 128, 128, 255) - UNORM for data textures
	unsigned char normalPixel[] = {128, 128, 255, 255};
	_defaultNormalTexture = std::make_shared<Texture>(_device, normalPixel, 1, 1, vk::Format::eR8G8B8A8Unorm);

	_textureCache["__default_white__"] = _defaultWhiteTexture;
	_textureCache["__default_normal__"] = _defaultNormalTexture;

	// Create default cubemap for IBL placeholder (1x1 black cubemap)
	_defaultCubemap = CubemapTexture::createDefault(_device);

	LogSystem::get().info("Default textures created");
}

std::shared_ptr<Texture> ResourceManager::getDefaultWhiteTexture() {
	if (!_defaultWhiteTexture) {
		createDefaultTextures();
	}
	return _defaultWhiteTexture;
}

std::shared_ptr<Texture> ResourceManager::getDefaultNormalTexture() {
	if (!_defaultNormalTexture) {
		createDefaultTextures();
	}
	return _defaultNormalTexture;
}

std::shared_ptr<CubemapTexture> ResourceManager::getDefaultCubemap() {
	if (!_defaultCubemap) {
		createDefaultTextures();
	}
	return _defaultCubemap;
}

const vk::raii::ShaderModule& ResourceManager::getShader(const std::string& filepath) {
	// Check cache first
	auto it = _shaderCache.find(filepath);
	if (it != _shaderCache.end()) {
		return it->second;
	}

	// Not in cache, load from file
	LogSystem::get().info("Loading shader: {}", filepath);
	std::vector<char> code = readFile(filepath);
	vk::raii::ShaderModule module = createShaderModule(code);

	auto insertedIt = _shaderCache.emplace(filepath, std::move(module));
	return insertedIt.first->second;
}

std::shared_ptr<Texture> ResourceManager::getTexture(const std::string& filepath, vk::Format format) {
	if (filepath.empty()) {
		return nullptr;
	}

	// Include format in cache key to support same file with different formats
	std::string cacheKey = filepath + "_fmt" + std::to_string(static_cast<int>(format));

	// Check cache
	auto it = _textureCache.find(cacheKey);
	if (it != _textureCache.end()) {
		return it->second;
	}

	// Load new
	try {
		LogSystem::get().info("Loading texture: {}", filepath);
		auto texture = std::make_shared<Texture>(_device, filepath, format);
		_textureCache.emplace(cacheKey, texture);
		return texture;
	} catch (const std::exception& e) {
		LogSystem::get().error("Failed to load texture: {} Error: {}", filepath, e.what());
		std::cerr << "Failed to load texture: " << filepath << " Error: " << e.what() << std::endl;
		return nullptr;
	}
}

std::shared_ptr<Texture> ResourceManager::loadTextureFromMemory(const unsigned char* data, size_t size,
																const std::string& cacheKey, vk::Format format) {
	// Include format in cache key
	std::string fullCacheKey = cacheKey + "_fmt" + std::to_string(static_cast<int>(format));

	// Check cache
	auto it = _textureCache.find(fullCacheKey);
	if (it != _textureCache.end()) {
		return it->second;
	}

	// Load from memory
	try {
		LogSystem::get().info("Loading embedded texture: {} ({} bytes)", cacheKey, size);
		auto texture = std::make_shared<Texture>(_device, data, size, format);
		_textureCache.emplace(fullCacheKey, texture);
		return texture;
	} catch (const std::exception& e) {
		LogSystem::get().error("Failed to load embedded texture: {} Error: {}", cacheKey, e.what());
		std::cerr << "Failed to load embedded texture: " << cacheKey << " Error: " << e.what() << std::endl;
		return nullptr;
	}
}

GPUMaterial* ResourceManager::getOrCreateGPUMaterial(const Material* material) {
	if (!material) {
		return nullptr;
	}

	// Check cache
	auto it = _gpuMaterialCache.find(material);
	if (it != _gpuMaterialCache.end()) {
		return it->second.get();
	}

	if (!_descriptorPool || _materialLayout == nullptr) {
		throw std::runtime_error("ResourceManager::initMaterialResources must be called before getOrCreateGPUMaterial");
	}

	auto gpuMaterial = std::make_unique<GPUMaterial>();

	// Create uniform buffer for material parameters
	gpuMaterial->uniformBuffer = std::make_unique<VulkanBuffer>(
		_device, sizeof(MaterialUBO), vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	gpuMaterial->uniformBuffer->map();

	// Fill MaterialUBO
	MaterialUBO ubo{};
	ubo.baseColorFactor = material->params.baseColorFactor;
	ubo.metallicFactor = material->params.metallicFactor;
	ubo.roughnessFactor = material->params.roughnessFactor;
	ubo.normalScale = material->params.normalScale;
	ubo.occlusionStrength = material->params.occlusionStrength;
	ubo.emissiveFactor = glm::vec4(material->params.emissiveFactor, material->params.emissiveStrength);
	ubo.alphaCutoff = material->params.alphaCutoff;

	// Extension parameters
	ubo.clearcoatFactor = material->params.clearcoatFactor;
	ubo.clearcoatRoughnessFactor = material->params.clearcoatRoughnessFactor;
	ubo.transmissionFactor = material->params.transmissionFactor;
	ubo.ior = material->params.ior;

	// Flags: bit 0 = doubleSided, bits 1-2 = alphaMode, bits 3+ = texture presence
	uint32_t flags = 0;
	if (material->params.doubleSided)
		flags |= (1 << 0);
	flags |= (static_cast<uint32_t>(material->params.alphaMode) << 1);
	if (material->baseColorMap)
		flags |= (1 << 3);
	if (material->metallicRoughnessMap)
		flags |= (1 << 4);
	if (material->normalMap)
		flags |= (1 << 5);
	if (material->occlusionMap)
		flags |= (1 << 6);
	if (material->emissiveMap)
		flags |= (1 << 7);
	if (material->clearcoatMap)
		flags |= (1 << 8);
	if (material->clearcoatRoughnessMap)
		flags |= (1 << 9);
	if (material->clearcoatNormalMap)
		flags |= (1 << 10);
	if (material->transmissionMap)
		flags |= (1 << 11);
	ubo.flags = flags;

	gpuMaterial->uniformBuffer->upload(&ubo, sizeof(ubo));

	// Allocate descriptor set
	vk::DescriptorSetAllocateInfo allocInfo{
		.descriptorPool = **_descriptorPool, .descriptorSetCount = 1, .pSetLayouts = &_materialLayout};
	gpuMaterial->descriptorSet = std::move(vk::raii::DescriptorSets(*_device, allocInfo)[0]);

	// Get textures (use defaults if not present)
	// Core PBR textures
	auto baseColorTex = material->baseColorMap ? material->baseColorMap : _defaultWhiteTexture;
	auto metallicRoughnessTex = material->metallicRoughnessMap ? material->metallicRoughnessMap : _defaultWhiteTexture;
	auto normalTex = material->normalMap ? material->normalMap : _defaultNormalTexture;
	auto occlusionTex = material->occlusionMap ? material->occlusionMap : _defaultWhiteTexture;
	auto emissiveTex = material->emissiveMap ? material->emissiveMap : _defaultWhiteTexture;

	// Extension textures (clearcoat, transmission)
	auto clearcoatTex = material->clearcoatMap ? material->clearcoatMap : _defaultWhiteTexture;
	auto clearcoatRoughnessTex =
		material->clearcoatRoughnessMap ? material->clearcoatRoughnessMap : _defaultWhiteTexture;
	auto clearcoatNormalTex = material->clearcoatNormalMap ? material->clearcoatNormalMap : _defaultNormalTexture;
	auto transmissionTex = material->transmissionMap ? material->transmissionMap : _defaultWhiteTexture;

	// Write descriptor set - 9 textures at bindings 1-9
	std::vector<vk::DescriptorImageInfo> imageInfos = {
		{*baseColorTex->getSampler(), *baseColorTex->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
		{*metallicRoughnessTex->getSampler(), *metallicRoughnessTex->getImageView(),
		 vk::ImageLayout::eShaderReadOnlyOptimal},
		{*normalTex->getSampler(), *normalTex->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
		{*occlusionTex->getSampler(), *occlusionTex->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
		{*emissiveTex->getSampler(), *emissiveTex->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
		{*clearcoatTex->getSampler(), *clearcoatTex->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
		{*clearcoatRoughnessTex->getSampler(), *clearcoatRoughnessTex->getImageView(),
		 vk::ImageLayout::eShaderReadOnlyOptimal},
		{*clearcoatNormalTex->getSampler(), *clearcoatNormalTex->getImageView(),
		 vk::ImageLayout::eShaderReadOnlyOptimal},
		{*transmissionTex->getSampler(), *transmissionTex->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal},
	};

	std::vector<vk::WriteDescriptorSet> writes;
	writes.reserve(imageInfos.size() + 1);

	// Binding 0: Material UBO
	vk::DescriptorBufferInfo bufferInfo{
		.buffer = gpuMaterial->uniformBuffer->getBuffer(), .offset = 0, .range = sizeof(MaterialUBO)};
	writes.push_back({.dstSet = *gpuMaterial->descriptorSet,
					  .dstBinding = 0,
					  .dstArrayElement = 0,
					  .descriptorCount = 1,
					  .descriptorType = vk::DescriptorType::eUniformBuffer,
					  .pBufferInfo = &bufferInfo});

	// Bindings 1-9: Textures
	for (uint32_t i = 0; i < imageInfos.size(); ++i) {
		writes.push_back({.dstSet = *gpuMaterial->descriptorSet,
						  .dstBinding = i + 1,
						  .dstArrayElement = 0,
						  .descriptorCount = 1,
						  .descriptorType = vk::DescriptorType::eCombinedImageSampler,
						  .pImageInfo = &imageInfos[i]});
	}

	_device->updateDescriptorSets(writes, {});

	auto result = gpuMaterial.get();
	_gpuMaterialCache.emplace(material, std::move(gpuMaterial));

	return result;
}

void ResourceManager::clearGPUResources() {
	// Idempotent: safe to call multiple times
	if (_gpuMaterialCache.empty()) {
		return; // Already cleared
	}

	LogSystem::get().info("Clearing GPU resources ({} materials)...", _gpuMaterialCache.size());
	// Clear GPU resources that hold descriptor sets from Renderer's pool
	// Must be called before Renderer destroys its descriptor pool
	_gpuMaterialCache.clear();
}

void ResourceManager::clear() {
	LogSystem::get().info("Clearing ResourceManager...");
	// Clear GPU resources first
	clearGPUResources();

	// Then textures and shaders
	_textureCache.clear();
	_shaderCache.clear();

	_defaultWhiteTexture.reset();
	_defaultNormalTexture.reset();

	LogSystem::get().info("ResourceManager cleared");
}

std::vector<char> ResourceManager::readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::string cwd = std::filesystem::current_path().string();
		throw std::runtime_error("failed to open file: " + filename + " (cwd: " + cwd + ")");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

vk::raii::ShaderModule ResourceManager::createShaderModule(const std::vector<char>& code) {
	vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size(),
										  .pCode = reinterpret_cast<const uint32_t*>(code.data())};

	return vk::raii::ShaderModule(*_device, createInfo);
}

} // namespace Fishy
