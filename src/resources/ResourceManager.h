#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include "../core/VulkanBuffer.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Fishy {

class VulkanDevice;
class Texture;
class CubemapTexture;
class Mesh;
class Material;

/**
 * @brief GPU representation of a Material.
 */
struct GPUMaterial {
	vk::raii::DescriptorSet descriptorSet = nullptr;
	std::unique_ptr<VulkanBuffer> uniformBuffer;
};

/**
 * @brief Manages loading and caching of resources including shaders, textures, and GPU resources.
 */
class ResourceManager {
public:
	explicit ResourceManager(VulkanDevice& device);
	~ResourceManager();

	/**
	 * @brief Initialize material descriptor resources.
	 * Must be called after Renderer creates descriptor layouts.
	 */
	void initMaterialResources(vk::DescriptorSetLayout materialLayout, const vk::raii::DescriptorPool& descriptorPool);

	// Shader loading
	const vk::raii::ShaderModule& getShader(const std::string& filepath);

	// Texture loading
	[[nodiscard]] std::shared_ptr<Texture> getTexture(const std::string& filepath,
													  vk::Format format = vk::Format::eR8G8B8A8Srgb);
	[[nodiscard]] std::shared_ptr<Texture> loadTextureFromMemory(const unsigned char* data, size_t size,
																 const std::string& cacheKey,
																 vk::Format format = vk::Format::eR8G8B8A8Srgb);

	// GPU resource management
	[[nodiscard]] GPUMaterial* getOrCreateGPUMaterial(const Material* material);

	// Get default white texture for materials without textures
	[[nodiscard]] std::shared_ptr<Texture> getDefaultWhiteTexture();
	[[nodiscard]] std::shared_ptr<Texture> getDefaultNormalTexture();

	// Get default cubemap for IBL placeholder bindings
	[[nodiscard]] std::shared_ptr<CubemapTexture> getDefaultCubemap();

	// Clear GPU resources (call before Renderer destroys descriptor pool)
	void clearGPUResources();

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
	std::unordered_map<const Material*, std::unique_ptr<GPUMaterial>> _gpuMaterialCache;

	// Material descriptor allocation
	vk::DescriptorSetLayout _materialLayout = nullptr;
	const vk::raii::DescriptorPool* _descriptorPool = nullptr;

	// Default textures
	std::shared_ptr<Texture> _defaultWhiteTexture;
	std::shared_ptr<Texture> _defaultNormalTexture;
	std::shared_ptr<CubemapTexture> _defaultCubemap;
};

} // namespace Fishy
