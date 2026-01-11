#pragma once

#include "ecs/Entity.h"
#include <entt/entt.hpp>

#include "Texture.h"
#include "core/Result.h"
#include "ecs/components/TransformComponent.h"
#include "rendering/Material.h"
#include "tiny_gltf.h"
#include <string>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

namespace Fishy {

class ResourceManager;
class Scene;

class ModelLoader {
public:
	/**
	 * @brief Load a glTF model directly into a Scene as entities.
	 *
	 * Each primitive in the model becomes an entity with MeshComponent,
	 * MeshRendererComponent, and TransformComponent.
	 *
	 * @param filepath Path to the glTF file.
	 * @param scene The scene to load entities into.
	 * @param resourceManager ResourceManager for texture loading.
	 * @return True if loading succeeded.
	 */
	[[nodiscard]] static Result<std::vector<Entity>> loadModelIntoScene(const std::string& filepath, Scene& scene,
																		ResourceManager& resourceManager);

private:
	// Forward declarations for internal use
	struct LoadContext;

	// Internal helper functions
	static entt::resource<Texture> loadGltfTexture(const LoadContext& ctx, int textureIndex, vk::Format format,
												   const std::string& texName);
	static entt::resource<Material> loadGltfMaterial(const LoadContext& ctx, int materialIndex);
	static void processGltfNode(LoadContext& ctx, int nodeIndex, Entity parentEntity = Entity{});

	static void applyNodeTransform(const tinygltf::Node& node, TransformComponent& transform);
};

} // namespace Fishy
