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

ResourceManager::ResourceManager(VulkanDevice& device) : _device(device) {}

ResourceManager::~ResourceManager() { clear(); }

// Bindless Resource Management

void ResourceManager::initBindlessResources(vk::DescriptorSet textureSet, vk::DescriptorSet ssboSet) {
	LogSystem::get().info("[Bindless] Initializing ResourceManager with EnTT resource caches...");

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
	_textureIdToBindlessHandle.clear();
	_texturePtrToBindlessHandle.clear();

	// Create default textures and register them
	createDefaultTextures();

	LogSystem::get().info("[Bindless] ResourceManager ready (max {} resources)", MAX_BINDLESS_RESOURCES);
}

// ========== Texture API ==========

entt::resource<Texture> ResourceManager::loadTexture(entt::id_type id, const std::string& filepath, vk::Format format) {
	// Check if already cached
	if (_textureCache.contains(id)) {
		return _textureCache[id];
	}

	LogSystem::get().info("Loading texture: {}", filepath);

	// Load using the TextureLoader - load() returns pair<iterator, bool>
	auto [it, inserted] = _textureCache.load(id, _device, filepath, format);

	// Register to bindless array using the shared_ptr from resource via .handle()
	if (_bindlessTextureSet) {
		auto bindlessHandle = registerTextureBindless(it->second.handle());
		_textureIdToBindlessHandle[id] = bindlessHandle;
	}

	// Return resource handle from cache
	return _textureCache[id];
}

entt::resource<Texture> ResourceManager::loadTextureFromMemory(entt::id_type id, const unsigned char* data, size_t size,
															   vk::Format format) {
	// Check if already cached
	if (_textureCache.contains(id)) {
		return _textureCache[id];
	}

	LogSystem::get().info("Loading embedded texture: id={} ({} bytes)", id, size);

	// Load using the TextureLoader (encoded data overload)
	auto [it, inserted] = _textureCache.load(id, _device, data, size, format);

	// Register to bindless array using .handle() to get shared_ptr
	if (_bindlessTextureSet) {
		auto bindlessHandle = registerTextureBindless(it->second.handle());
		_textureIdToBindlessHandle[id] = bindlessHandle;
	}

	// Return resource handle from cache
	return _textureCache[id];
}

entt::resource<Texture> ResourceManager::getTexture(entt::id_type id) const {
	// Note: const operator[] returns resource<const T>, we need to cast for the return type
	// This is safe because we're just providing read access to the resource handle
	if (_textureCache.contains(id)) {
		return const_cast<entt::resource_cache<Texture, TextureLoader>&>(_textureCache)[id];
	}
	return {};
}

uint32_t ResourceManager::getTextureBindlessIndex(entt::id_type id) const {
	auto it = _textureIdToBindlessHandle.find(id);
	if (it != _textureIdToBindlessHandle.end()) {
		return it->second.index;
	}
	return UINT32_MAX;
}

uint32_t ResourceManager::getTextureBindlessIndex(const Texture* texture) const {
	if (!texture) {
		return UINT32_MAX;
	}
	auto it = _texturePtrToBindlessHandle.find(texture);
	if (it != _texturePtrToBindlessHandle.end()) {
		return it->second.index;
	}
	return UINT32_MAX;
}

TextureHandle ResourceManager::registerTextureBindless(const std::shared_ptr<Texture>& texture) {
	if (!texture) {
		return TextureHandle{};
	}

	// Check if already registered by pointer
	auto existingIt = _texturePtrToBindlessHandle.find(texture.get());
	if (existingIt != _texturePtrToBindlessHandle.end()) {
		return existingIt->second;
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

	// Store mapping by pointer for reverse lookup
	TextureHandle handle{index};
	_texturePtrToBindlessHandle[texture.get()] = handle;

	return handle;
}

// Default Textures

void ResourceManager::createDefaultTextures() {
	LogSystem::get().info("Creating default textures (white, normal)...");

	// Create 1x1 white texture (RGBA) - sRGB for color data
	unsigned char whitePixel[] = {255, 255, 255, 255};
	auto [whiteIt, whiteInserted] =
		_textureCache.load(DEFAULT_WHITE_TEXTURE_ID, _device, whitePixel, 1, 1, vk::Format::eR8G8B8A8Srgb);
	if (_bindlessTextureSet) {
		auto bindlessHandle = registerTextureBindless(whiteIt->second.handle());
		_textureIdToBindlessHandle[DEFAULT_WHITE_TEXTURE_ID] = bindlessHandle;
	}

	// Create 1x1 default normal map (pointing up: RGB = 128, 128, 255) - UNORM for data textures
	unsigned char normalPixel[] = {128, 128, 255, 255};
	auto [normalIt, normalInserted] =
		_textureCache.load(DEFAULT_NORMAL_TEXTURE_ID, _device, normalPixel, 1, 1, vk::Format::eR8G8B8A8Unorm);
	if (_bindlessTextureSet) {
		auto bindlessHandle = registerTextureBindless(normalIt->second.handle());
		_textureIdToBindlessHandle[DEFAULT_NORMAL_TEXTURE_ID] = bindlessHandle;
	}

	// Create default cubemap for IBL placeholder (1x1 black cubemap)
	_defaultCubemap = CubemapTexture::createDefault(_device);

	LogSystem::get().info("Default textures created");
}

entt::resource<Texture> ResourceManager::getDefaultWhiteTexture() {
	if (!_textureCache.contains(DEFAULT_WHITE_TEXTURE_ID)) {
		createDefaultTextures();
	}
	return _textureCache[DEFAULT_WHITE_TEXTURE_ID];
}

entt::resource<Texture> ResourceManager::getDefaultNormalTexture() {
	if (!_textureCache.contains(DEFAULT_NORMAL_TEXTURE_ID)) {
		createDefaultTextures();
	}
	return _textureCache[DEFAULT_NORMAL_TEXTURE_ID];
}

std::shared_ptr<CubemapTexture> ResourceManager::getDefaultCubemap() {
	if (!_defaultCubemap) {
		createDefaultTextures();
	}
	return _defaultCubemap;
}

// ========== Mesh API ==========

entt::resource<Mesh> ResourceManager::createMesh(entt::id_type id, std::vector<Vertex>& vertices,
												 std::vector<uint32_t>& indices) {
	// Check if already cached
	if (_meshCache.contains(id)) {
		return _meshCache[id];
	}

	LogSystem::get().trace("Creating mesh: id={}", id);
	_meshCache.load(id, vertices, indices);
	return _meshCache[id];
}

entt::resource<Mesh> ResourceManager::getMesh(entt::id_type id) const {
	if (_meshCache.contains(id)) {
		return const_cast<entt::resource_cache<Mesh, MeshLoader>&>(_meshCache)[id];
	}
	return {};
}

// ========== Material API ==========

entt::resource<Material> ResourceManager::createMaterial(entt::id_type id) {
	// Check if already cached
	if (_materialCache.contains(id)) {
		return _materialCache[id];
	}

	LogSystem::get().trace("Creating material: id={}", id);
	_materialCache.load(id);
	return _materialCache[id];
}

entt::resource<Material> ResourceManager::getMaterial(entt::id_type id) const {
	if (_materialCache.contains(id)) {
		return const_cast<entt::resource_cache<Material, MaterialLoader>&>(_materialCache)[id];
	}
	return {};
}

// ========== Buffer API ==========

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

// ========== Shader API ==========

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

// Lifecycle

void ResourceManager::clear() {
	LogSystem::get().info("Clearing ResourceManager...");

	// Clear EnTT caches
	_textureCache.clear();
	_meshCache.clear();
	_materialCache.clear();

	// Clear shader cache
	_shaderCache.clear();

	// Clear bindless mappings
	_textureIdToBindlessHandle.clear();
	_texturePtrToBindlessHandle.clear();

	// Reset cubemap
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
