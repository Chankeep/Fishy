#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include "../core/VulkanBuffer.h"
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace Fishy {

class VulkanDevice;
class Texture;
class CubemapTexture;
class Mesh;
class Material;

// Handle for a texture registered in the bindless array
struct TextureHandle {
	uint32_t index = UINT32_MAX;
	[[nodiscard]] bool isValid() const { return index != UINT32_MAX; }
};

// Handle for a buffer registered in the bindless SSBO array
struct BufferHandle {
	uint32_t index = UINT32_MAX;
	[[nodiscard]] bool isValid() const { return index != UINT32_MAX; }
};

// @deprecated Use bindless texture indices instead
struct [[deprecated("Use bindless texture indices instead")]] GPUMaterial {
	vk::raii::DescriptorSet descriptorSet = nullptr;
	std::unique_ptr<VulkanBuffer> uniformBuffer;
};

/**
 * @brief Manages loading and caching of resources including shaders, textures, and GPU resources.
 *
 * Supports bindless resource management for textures and storage buffers.
 */
class ResourceManager {
public:
	explicit ResourceManager(VulkanDevice& device);
	~ResourceManager();
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;

	// Initialize bindless descriptor sets (called by Renderer after allocation)
	void initBindlessResources(vk::DescriptorSet textureSet, vk::DescriptorSet ssboSet);

	// Register a texture to the bindless array, returns handle with index
	[[nodiscard]] TextureHandle registerTexture(std::shared_ptr<Texture> texture);

	// Unregister a texture and recycle its slot
	void unregisterTexture(TextureHandle handle);

	/// Register a buffer to the bindless SSBO array
	[[nodiscard]] BufferHandle registerBuffer(VulkanBuffer* buffer, vk::DeviceSize size);

	// Unregister a buffer and recycle its slot
	void unregisterBuffer(BufferHandle handle);

	// Get the bindless index for a texture (returns UINT32_MAX if not registered)
	[[nodiscard]] uint32_t getTextureIndex(const std::shared_ptr<Texture>& texture) const;

	// Load or get cached shader module
	[[nodiscard]] const vk::raii::ShaderModule& getShader(const std::string& filepath);

	// Load or get cached texture (auto-registers to bindless if enabled)
	[[nodiscard]] std::shared_ptr<Texture> getTexture(const std::string& filepath,
													  vk::Format format = vk::Format::eR8G8B8A8Srgb);

	// Load texture from memory (auto-registers to bindless if enabled)
	[[nodiscard]] std::shared_ptr<Texture> loadTextureFromMemory(const unsigned char* data, size_t size,
																 const std::string& cacheKey,
																 vk::Format format = vk::Format::eR8G8B8A8Srgb);

	// Default Resources
	[[nodiscard]] std::shared_ptr<Texture> getDefaultWhiteTexture();
	[[nodiscard]] std::shared_ptr<Texture> getDefaultNormalTexture();
	[[nodiscard]] std::shared_ptr<CubemapTexture> getDefaultCubemap();

	// Clear all cached resources
	void clear();

private:
	// Helper functions
	static std::vector<char> readFile(const std::string& filename);
	vk::raii::ShaderModule createShaderModule(const std::vector<char>& code);
	void createDefaultTextures();

	// Core dependency
	VulkanDevice& _device;

	// Caches
	std::unordered_map<std::string, vk::raii::ShaderModule> _shaderCache;
	std::unordered_map<std::string, std::shared_ptr<Texture>> _textureCache;

	// Default textures
	std::shared_ptr<Texture> _defaultWhiteTexture;
	std::shared_ptr<Texture> _defaultNormalTexture;
	std::shared_ptr<CubemapTexture> _defaultCubemap;

	static constexpr uint32_t MAX_BINDLESS_RESOURCES = 4096;

	vk::DescriptorSet _bindlessTextureSet = nullptr;
	vk::DescriptorSet _bindlessStorageBufferSet = nullptr;

	// Texture slot management
	std::queue<uint32_t> _freeTextureSlots;
	uint32_t _nextTextureIndex = 0;
	std::unordered_map<Texture*, TextureHandle> _textureToHandle;

	// Buffer slot management
	std::queue<uint32_t> _freeBufferSlots;
	uint32_t _nextBufferIndex = 0;
};

} // namespace Fishy
