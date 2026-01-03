#include "ResourceManager.h"
#include "../core/VulkanDevice.h"
#include "../rendering/Material.h"
#include "CubemapTexture.h"
#include "Mesh.h"
#include "Texture.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace Fishy {

namespace MaterialFlags {
constexpr uint32_t DoubleSided = 1u << 0;
constexpr uint32_t AlphaModeShift = 1;
constexpr uint32_t HasBaseColorMap = 1u << 3;
constexpr uint32_t HasMetallicRoughnessMap = 1u << 4;
constexpr uint32_t HasNormalMap = 1u << 5;
constexpr uint32_t HasOcclusionMap = 1u << 6;
constexpr uint32_t HasEmissiveMap = 1u << 7;
constexpr uint32_t HasClearcoatMap = 1u << 8;
constexpr uint32_t HasClearcoatRoughnessMap = 1u << 9;
constexpr uint32_t HasClearcoatNormalMap = 1u << 10;
constexpr uint32_t HasTransmissionMap = 1u << 11;
} // namespace MaterialFlags

ResourceManager::ResourceManager(VulkanDevice& device) : _device(device) {}

ResourceManager::~ResourceManager() { clear(); }

// Bindless Resource Management

void ResourceManager::initBindlessResources(vk::DescriptorSet textureSet, vk::DescriptorSet ssboSet) {
	LogSystem::get().info("[Bindless] Initializing ResourceManager...");

	_bindlessTextureSet = textureSet;
	_bindlessStorageBufferSet = ssboSet;

	// Reset slot allocators
	_nextTextureIndex = 0;
	_nextBufferIndex = 0;

	// Clear recycle queues
	while (!_freeTextureSlots.empty())
		_freeTextureSlots.pop();
	while (!_freeBufferSlots.empty())
		_freeBufferSlots.pop();

	// Clear handle mappings
	_textureToHandle.clear();

	// Create default textures and register them
	createDefaultTextures();

	LogSystem::get().info("[Bindless] ResourceManager ready (max {} resources)", MAX_BINDLESS_RESOURCES);
}

TextureHandle ResourceManager::registerTexture(std::shared_ptr<Texture> texture) {
	if (!texture) {
		return TextureHandle{};
	}

	// Check if already registered
	auto it = _textureToHandle.find(texture.get());
	if (it != _textureToHandle.end()) {
		return it->second;
	}

	// Check if bindless is initialized
	if (!_bindlessTextureSet) {
		LogSystem::get().warn("[Bindless] Cannot register texture - bindless not initialized");
		return TextureHandle{};
	}

	// Allocate slot (prefer recycled slots)
	uint32_t index;
	if (!_freeTextureSlots.empty()) {
		index = _freeTextureSlots.front();
		_freeTextureSlots.pop();
		LogSystem::get().trace("[Bindless] Reusing texture slot {}", index);
	} else {
		if (_nextTextureIndex >= MAX_BINDLESS_RESOURCES) {
			LogSystem::get().error("[Bindless] Texture array full!");
			return TextureHandle{};
		}
		index = _nextTextureIndex++;
		LogSystem::get().trace("[Bindless] Allocated new texture slot {}", index);
	}

	// Write descriptor (UPDATE_AFTER_BIND allows updating while set is bound!)
	vk::DescriptorImageInfo imageInfo{.sampler = *texture->getSampler(),
									  .imageView = *texture->getImageView(),
									  .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

	vk::WriteDescriptorSet write{.dstSet = _bindlessTextureSet,
								 .dstBinding = 0,
								 .dstArrayElement = index,
								 .descriptorCount = 1,
								 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
								 .pImageInfo = &imageInfo};

	_device->updateDescriptorSets(write, {});

	// Store mapping
	TextureHandle handle{index};
	_textureToHandle[texture.get()] = handle;

	return handle;
}

void ResourceManager::unregisterTexture(TextureHandle handle) {
	if (!handle.isValid()) {
		return;
	}

	// Recycle slot
	_freeTextureSlots.push(handle.index);

	// Remove from mapping (reverse lookup)
	for (auto it = _textureToHandle.begin(); it != _textureToHandle.end(); ++it) {
		if (it->second.index == handle.index) {
			_textureToHandle.erase(it);
			break;
		}
	}

	LogSystem::get().trace("[Bindless] Freed texture slot {}", handle.index);
}

BufferHandle ResourceManager::registerBuffer(VulkanBuffer* buffer, vk::DeviceSize size) {
	if (!buffer) {
		return BufferHandle{};
	}

	if (!_bindlessStorageBufferSet) {
		LogSystem::get().warn("[Bindless] Cannot register buffer - bindless not initialized");
		return BufferHandle{};
	}

	// Allocate slot
	uint32_t index;
	if (!_freeBufferSlots.empty()) {
		index = _freeBufferSlots.front();
		_freeBufferSlots.pop();
	} else {
		if (_nextBufferIndex >= MAX_BINDLESS_RESOURCES) {
			LogSystem::get().error("[Bindless] Buffer array full!");
			return BufferHandle{};
		}
		index = _nextBufferIndex++;
	}

	// Write descriptor
	vk::DescriptorBufferInfo bufferInfo{.buffer = buffer->getBuffer(), .offset = 0, .range = size};

	vk::WriteDescriptorSet write{.dstSet = _bindlessStorageBufferSet,
								 .dstBinding = 0,
								 .dstArrayElement = index,
								 .descriptorCount = 1,
								 .descriptorType = vk::DescriptorType::eStorageBuffer,
								 .pBufferInfo = &bufferInfo};

	_device->updateDescriptorSets(write, {});

	LogSystem::get().trace("[Bindless] Registered buffer at slot {}", index);
	return BufferHandle{index};
}

void ResourceManager::unregisterBuffer(BufferHandle handle) {
	if (!handle.isValid()) {
		return;
	}

	_freeBufferSlots.push(handle.index);
	LogSystem::get().trace("[Bindless] Freed buffer slot {}", handle.index);
}

uint32_t ResourceManager::getTextureIndex(const std::shared_ptr<Texture>& texture) const {
	if (!texture) {
		return UINT32_MAX;
	}

	auto it = _textureToHandle.find(texture.get());
	if (it != _textureToHandle.end()) {
		return it->second.index;
	}

	return UINT32_MAX;
}

// Default Textures

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

	// Auto-register default textures to bindless if initialized
	if (_bindlessTextureSet) {
		(void)registerTexture(_defaultWhiteTexture);
		(void)registerTexture(_defaultNormalTexture);
	}

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

// Resource Loading

const vk::raii::ShaderModule& ResourceManager::getShader(const std::string& filepath) {
	auto it = _shaderCache.find(filepath);
	if (it != _shaderCache.end()) {
		return it->second;
	}

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

	std::string cacheKey = filepath + "_fmt" + std::to_string(static_cast<int>(format));

	auto it = _textureCache.find(cacheKey);
	if (it != _textureCache.end()) {
		return it->second;
	}

	try {
		LogSystem::get().info("Loading texture: {}", filepath);
		auto texture = std::make_shared<Texture>(_device, filepath, format);
		_textureCache.emplace(cacheKey, texture);

		// Auto-register to bindless if enabled
		if (_bindlessTextureSet) {
			(void)registerTexture(texture);
		}

		return texture;
	} catch (const std::exception& e) {
		LogSystem::get().error("Failed to load texture: {} Error: {}", filepath, e.what());
		return nullptr;
	}
}

std::shared_ptr<Texture> ResourceManager::loadTextureFromMemory(const unsigned char* data, size_t size,
																const std::string& cacheKey, vk::Format format) {
	std::string fullCacheKey = cacheKey + "_fmt" + std::to_string(static_cast<int>(format));

	auto it = _textureCache.find(fullCacheKey);
	if (it != _textureCache.end()) {
		return it->second;
	}

	try {
		LogSystem::get().info("Loading embedded texture: {} ({} bytes)", cacheKey, size);
		auto texture = std::make_shared<Texture>(_device, data, size, format);
		_textureCache.emplace(fullCacheKey, texture);

		// Auto-register to bindless if enabled
		if (_bindlessTextureSet) {
			(void)registerTexture(texture);
		}

		return texture;
	} catch (const std::exception& e) {
		LogSystem::get().error("Failed to load embedded texture: {} Error: {}", cacheKey, e.what());
		return nullptr;
	}
}

// Lifecycle

void ResourceManager::clear() {
	LogSystem::get().info("Clearing ResourceManager...");

	_textureCache.clear();
	_shaderCache.clear();
	_textureToHandle.clear();

	_defaultWhiteTexture.reset();
	_defaultNormalTexture.reset();
	_defaultCubemap.reset();

	// Reset bindless state
	_bindlessTextureSet = nullptr;
	_bindlessStorageBufferSet = nullptr;
	_nextTextureIndex = 0;
	_nextBufferIndex = 0;
	while (!_freeTextureSlots.empty())
		_freeTextureSlots.pop();
	while (!_freeBufferSlots.empty())
		_freeBufferSlots.pop();

	LogSystem::get().info("ResourceManager cleared");
}

// Helper Functions

std::vector<char> ResourceManager::readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::string cwd = std::filesystem::current_path().string();
		throw std::runtime_error("failed to open file: " + filename + " (cwd: " + cwd + ")");
	}

	auto fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	return buffer;
}

vk::raii::ShaderModule ResourceManager::createShaderModule(const std::vector<char>& code) {
	vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size(),
										  .pCode = reinterpret_cast<const uint32_t*>(code.data())};

	return vk::raii::ShaderModule(*_device, createInfo);
}

} // namespace Fishy
