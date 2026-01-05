#pragma once

#include "Model.h"
#include <memory>
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
	// Loads a glTF model from the specified path.
	// Uses ResourceManager to load textures if provided.
	[[nodiscard]] static std::shared_ptr<Model> loadModel(const std::string& filepath,
														  ResourceManager* resourceManager = nullptr);

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
	static bool loadModelIntoScene(const std::string& filepath, Scene& scene,
								   ResourceManager* resourceManager = nullptr);

private:
	// Forward declarations for internal use
	struct LoadContext;

	// Helper functions extracted from loadModel
	static std::shared_ptr<Texture> loadGltfTexture(const LoadContext& ctx, int textureIndex, vk::Format format,
													const std::string& texName);
	static std::shared_ptr<Material> loadGltfMaterial(const LoadContext& ctx, int materialIndex);
	static void processGltfNode(const LoadContext& ctx, int nodeIndex, std::shared_ptr<Model>& model);
};

} // namespace Fishy
