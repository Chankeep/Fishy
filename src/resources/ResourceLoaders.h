#pragma once

#include "../core/VulkanDevice.h"
#include "../rendering/Material.h"
#include "Mesh.h"
#include "Texture.h"
#include <entt/entt.hpp>
#include <memory>
#include <span>

namespace Fishy {

class ResourceManager; // Forward declaration for loaders that need device/bindless access

/**
 * @brief Loader for Texture resources.
 *
 * This is the callable type used by entt::resource_cache<Texture>.
 * It creates the Texture object and uploads it to the GPU.
 */
struct TextureLoader final {
	using result_type = std::shared_ptr<Texture>;

	// Load texture from file path
	result_type operator()(const VulkanDevice& device, const std::string& path,
						   vk::Format format = vk::Format::eR8G8B8A8Srgb) const {
		return std::make_shared<Texture>(device, path, format);
	}

	// Load from raw encoded data (PNG/JPG bytes, for embedded glTF textures)
	result_type operator()(const VulkanDevice& device, std::span<const std::byte> data, vk::Format format) const {
		return std::make_shared<Texture>(device, data, format);
	}

	// Create from raw RGBA pixels (for programmatic default textures)
	result_type operator()(const VulkanDevice& device, const unsigned char* pixels, int width, int height,
						   vk::Format format) const {
		return std::make_shared<Texture>(device, pixels, width, height, format);
	}
};

/**
 * @brief Loader for Mesh resources.
 */
struct MeshLoader final {
	using result_type = std::shared_ptr<Mesh>;

	result_type operator()(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) const {
		return std::make_shared<Mesh>(vertices, indices);
	}
};

/**
 * @brief Loader for Material resources.
 *
 * Materials are typically created by copying or from a default,
 * then configured post-creation.
 */
struct MaterialLoader final {
	using result_type = std::shared_ptr<Material>;

	result_type operator()() const { return std::make_shared<Material>(); }
};

} // namespace Fishy
