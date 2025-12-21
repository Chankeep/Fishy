#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <string>
#include <unordered_map>
#include <vector>

namespace Fishy {

class VulkanDevice;
class Texture;

/**
 * @brief Manages loading and caching of resources like shaders.
 */
class ResourceManager {
public:
	explicit ResourceManager(VulkanDevice& device);
	~ResourceManager();

	// Load shader from file path. If already loaded, returns cached module.
	const vk::raii::ShaderModule& getShader(const std::string& filepath);

	// Load texture from file path. If already loaded, returns cached texture.
	std::shared_ptr<Texture> getTexture(const std::string& filepath);

	// Clear all cached resources
	void clear();

private:
	// Helper to load binary data from file
	static std::vector<char> readFile(const std::string& filename);

	// Helper to create shader module
	vk::raii::ShaderModule createShaderModule(const std::vector<char>& code);

private:
	VulkanDevice& _device;
	std::unordered_map<std::string, vk::raii::ShaderModule> _shaderCache;
	std::unordered_map<std::string, std::shared_ptr<Texture>> _textureCache;
};

} // namespace Fishy
