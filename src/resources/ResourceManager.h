#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include "../core/VulkanBuffer.h"
#include "CubemapTexture.h"
#include "ResourceLoaders.h"
#include <entt/entt.hpp>
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

/**
 * @brief Manages loading and caching of resources using EnTT's resource system.
 *
 * Supports bindless resource management for textures and storage buffers.
 * Resources are accessed via entt::resource handles instead of raw pointers.
 */
class ResourceManager {
public:
	explicit ResourceManager(VulkanDevice& device);
	~ResourceManager();
	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;

	// Initialize bindless descriptor sets (called by Renderer after allocation)
	void initBindlessResources(vk::DescriptorSet textureSet, vk::DescriptorSet ssboSet);

	// ========== Texture API ==========
	/**
	 * @brief Load or get a cached texture from file.
	 * @param id Unique identifier (typically the file path or a hashed key).
	 * @param filepath Path to the texture file.
	 * @param format Vulkan format for the texture.
	 * @return entt::resource handle to the texture.
	 */
	[[nodiscard]] entt::resource<Texture> loadTexture(entt::id_type id, const std::string& filepath,
													  vk::Format format = vk::Format::eR8G8B8A8Srgb);

	/**
	 * @brief Load a texture from embedded memory data.
	 * @param id Unique identifier for caching.
	 * @param data Pointer to encoded image data (PNG/JPG).
	 * @param size Size of the data in bytes.
	 * @param format Vulkan format.
	 * @return entt::resource handle to the texture.
	 */
	[[nodiscard]] entt::resource<Texture> loadTextureFromMemory(entt::id_type id, const unsigned char* data,
																size_t size, vk::Format format);

	/**
	 * @brief Get a previously loaded texture by its ID.
	 * @param id The identifier used during loading.
	 * @return entt::resource handle, or empty if not found.
	 */
	[[nodiscard]] entt::resource<Texture> getTexture(entt::id_type id) const;

	/**
	 * @brief Get the bindless descriptor index for a texture.
	 * @param id The texture's cache ID.
	 * @return Bindless array index, or UINT32_MAX if not found.
	 */
	[[nodiscard]] uint32_t getTextureBindlessIndex(entt::id_type id) const;

	/**
	 * @brief Get the bindless descriptor index for a texture by pointer.
	 * @param texture Pointer to the texture (from entt::resource.get()).
	 * @return Bindless array index, or UINT32_MAX if not found.
	 */
	[[nodiscard]] uint32_t getTextureBindlessIndex(const Texture* texture) const;

	// Default textures
	[[nodiscard]] entt::resource<Texture> getDefaultWhiteTexture();
	[[nodiscard]] entt::resource<Texture> getDefaultNormalTexture();
	[[nodiscard]] std::shared_ptr<CubemapTexture> getDefaultCubemap();

	// ========== Mesh API ==========
	/**
	 * @brief Create and cache a mesh with the given geometry.
	 * @param id Unique identifier for the mesh.
	 * @param vertices Vertex data.
	 * @param indices Index data.
	 * @return entt::resource handle to the mesh.
	 */
	[[nodiscard]] entt::resource<Mesh> createMesh(entt::id_type id, std::vector<Vertex>& vertices,
												  std::vector<uint32_t>& indices);

	/**
	 * @brief Get a previously created mesh by its ID.
	 */
	[[nodiscard]] entt::resource<Mesh> getMesh(entt::id_type id) const;

	// ========== Material API ==========
	/**
	 * @brief Create and cache a new material.
	 * @param id Unique identifier for the material.
	 * @return entt::resource handle to the material.
	 */
	[[nodiscard]] entt::resource<Material> createMaterial(entt::id_type id);

	/**
	 * @brief Get a previously created material by its ID.
	 */
	[[nodiscard]] entt::resource<Material> getMaterial(entt::id_type id) const;

	// ========== Bindless Buffer API ==========
	/// Register a buffer to the bindless SSBO array
	[[nodiscard]] BufferHandle registerBuffer(VulkanBuffer* buffer, vk::DeviceSize size);

	// Unregister a buffer and recycle its slot
	void unregisterBuffer(BufferHandle handle);

	// ========== Shader API (unchanged) ==========
	[[nodiscard]] const vk::raii::ShaderModule& getShader(const std::string& filepath);

	// Clear all cached resources
	void clear();

	// Access to device (for loaders that need it)
	[[nodiscard]] VulkanDevice& getDevice() { return _device; }
	[[nodiscard]] const VulkanDevice& getDevice() const { return _device; }

private:
	// Helper functions
	static std::vector<char> readFile(const std::string& filename);
	vk::raii::ShaderModule createShaderModule(const std::vector<char>& code);
	void createDefaultTextures();
	TextureHandle registerTextureBindless(const std::shared_ptr<Texture>& texture);

	// Core dependency
	VulkanDevice& _device;

	// EnTT Resource Caches
	entt::resource_cache<Texture, TextureLoader> _textureCache;
	entt::resource_cache<Mesh, MeshLoader> _meshCache;
	entt::resource_cache<Material, MaterialLoader> _materialCache;

	// Shader cache (separate, shaders don't need EnTT resource semantics)
	std::unordered_map<std::string, vk::raii::ShaderModule> _shaderCache;

	// Default texture IDs (using hashed strings)
	static constexpr entt::id_type DEFAULT_WHITE_TEXTURE_ID = entt::hashed_string{"__default_white__"};
	static constexpr entt::id_type DEFAULT_NORMAL_TEXTURE_ID = entt::hashed_string{"__default_normal__"};

	// Default cubemap (still shared_ptr for simplicity, cubemaps are special resources)
	std::shared_ptr<CubemapTexture> _defaultCubemap;

	// Bindless descriptor sets
	static constexpr uint32_t MAX_BINDLESS_RESOURCES = 4096;
	vk::DescriptorSet _bindlessTextureSet = nullptr;
	vk::DescriptorSet _bindlessStorageBufferSet = nullptr;

	// Texture slot management for bindless
	std::queue<uint32_t> _freeTextureSlots;
	uint32_t _nextTextureIndex = 0;
	std::unordered_map<entt::id_type, TextureHandle> _textureIdToBindlessHandle;
	std::unordered_map<const Texture*, TextureHandle> _texturePtrToBindlessHandle;

	// Buffer slot management
	std::queue<uint32_t> _freeBufferSlots;
	uint32_t _nextBufferIndex = 0;
};

} // namespace Fishy
