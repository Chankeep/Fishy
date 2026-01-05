#include "ModelLoader.h"
#include "../core/LogSystem.h"
#include "../ecs/Entity.h"
#include "../ecs/components/MeshComponent.h"
#include "../ecs/components/MeshRendererComponent.h"
#include "../scene/Scene.h"
#include "ResourceManager.h"


#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// STB_IMAGE_IMPLEMENTATION is defined in Texture.cpp

#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <tiny_gltf.h>

namespace Fishy {

// glTF extension names
namespace GltfExtensions {
constexpr const char* CLEARCOAT = "KHR_materials_clearcoat";
constexpr const char* TRANSMISSION = "KHR_materials_transmission";
constexpr const char* IOR = "KHR_materials_ior";
constexpr const char* VOLUME = "KHR_materials_volume";
constexpr const char* SHEEN = "KHR_materials_sheen";
constexpr const char* SPECULAR = "KHR_materials_specular";
constexpr const char* EMISSIVE_STRENGTH = "KHR_materials_emissive_strength";
constexpr const char* TEXTURE_BASISU = "KHR_texture_basisu";
constexpr const char* DRACO = "KHR_draco_mesh_compression";
constexpr const char* MESH_QUANTIZATION = "KHR_mesh_quantization";
} // namespace GltfExtensions

// Helper to join strings with a delimiter
static std::string joinStrings(const std::vector<std::string>& strings, const std::string& delimiter = ", ") {
	if (strings.empty())
		return "";
	std::string result = strings[0];
	for (size_t i = 1; i < strings.size(); ++i) {
		result += delimiter + strings[i];
	}
	return result;
}

// Log glTF model extensions for debugging
static void logGltfExtensions(const tinygltf::Model& gltfModel) {
	if (!gltfModel.extensionsUsed.empty()) {
		LogSystem::get().info("glTF extensions used:");
		for (const auto& ext : gltfModel.extensionsUsed) {
			LogSystem::get().info("  - {}", ext);
		}
	}

	if (!gltfModel.extensionsRequired.empty()) {
		LogSystem::get().info("glTF extensions required:");
		for (const auto& ext : gltfModel.extensionsRequired) {
			std::string extInfo = "  - " + ext;
			if (ext == GltfExtensions::CLEARCOAT || ext == GltfExtensions::TRANSMISSION || ext == GltfExtensions::IOR ||
				ext == GltfExtensions::VOLUME || ext == GltfExtensions::SHEEN || ext == GltfExtensions::SPECULAR ||
				ext == GltfExtensions::EMISSIVE_STRENGTH) {
				extInfo += " (material extension - partial support)";
			} else if (ext == GltfExtensions::TEXTURE_BASISU) {
				extInfo += " (requires KTX2/Basis Universal - NOT YET SUPPORTED)";
			} else if (ext == GltfExtensions::DRACO) {
				extInfo += " (requires Draco - NOT SUPPORTED)";
			} else if (ext == GltfExtensions::MESH_QUANTIZATION) {
				extInfo += " (mesh quantization - supported via tinygltf)";
			} else {
				extInfo += " (UNKNOWN)";
			}
			LogSystem::get().info("{}", extInfo);
		}
	}
}

// Compute tangents for vertices when they are not provided by the model
// Based on the algorithm from "Foundations of Game Engine Development, Volume 2: Rendering"
// by Eric Lengyel. This provides smooth per-vertex tangents.
static void computeTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
	// Allocate temporary arrays for tangent and bitangent accumulation
	std::vector<glm::vec3> tan1(vertices.size(), glm::vec3(0.0f));
	std::vector<glm::vec3> tan2(vertices.size(), glm::vec3(0.0f));

	// Process each triangle
	for (size_t i = 0; i < indices.size(); i += 3) {
		uint32_t i0 = indices[i];
		uint32_t i1 = indices[i + 1];
		uint32_t i2 = indices[i + 2];

		const glm::vec3& v0 = vertices[i0].pos;
		const glm::vec3& v1 = vertices[i1].pos;
		const glm::vec3& v2 = vertices[i2].pos;

		const glm::vec2& uv0 = vertices[i0].texCoord;
		const glm::vec2& uv1 = vertices[i1].texCoord;
		const glm::vec2& uv2 = vertices[i2].texCoord;

		// Edge vectors
		glm::vec3 e1 = v1 - v0;
		glm::vec3 e2 = v2 - v0;

		// UV deltas
		glm::vec2 duv1 = uv1 - uv0;
		glm::vec2 duv2 = uv2 - uv0;

		float r = 1.0f / (duv1.x * duv2.y - duv2.x * duv1.y + 1e-6f);

		glm::vec3 tangent = (e1 * duv2.y - e2 * duv1.y) * r;
		glm::vec3 bitangent = (e2 * duv1.x - e1 * duv2.x) * r;

		// Accumulate for each vertex of the triangle
		tan1[i0] += tangent;
		tan1[i1] += tangent;
		tan1[i2] += tangent;

		tan2[i0] += bitangent;
		tan2[i1] += bitangent;
		tan2[i2] += bitangent;
	}

	// Orthogonalize and compute handedness for each vertex
	for (size_t i = 0; i < vertices.size(); ++i) {
		const glm::vec3& n = vertices[i].normal;
		const glm::vec3& t = tan1[i];

		// Gram-Schmidt orthogonalize: T' = normalize(T - N * dot(N, T))
		glm::vec3 tangent = glm::normalize(t - n * glm::dot(n, t));

		// Calculate handedness: sign = dot(cross(N, T), B) < 0 ? -1 : 1
		float w = (glm::dot(glm::cross(n, t), tan2[i]) < 0.0f) ? -1.0f : 1.0f;

		vertices[i].tangent = glm::vec4(tangent, w);
	}

	LogSystem::get().trace("Computed {} vertex tangents", vertices.size());
}

// Context struct to hold shared state between helper functions
struct ModelLoader::LoadContext {
	const tinygltf::Model& gltfModel;
	const std::string& filepath;
	const std::string& baseDir;
	ResourceManager* resourceManager;
};

// Load texture from glTF model (external file, embedded buffer, or decoded image)
std::shared_ptr<Texture> ModelLoader::loadGltfTexture(const LoadContext& ctx, int textureIndex, vk::Format format,
													  const std::string& texName) {
	if (textureIndex < 0 || textureIndex >= static_cast<int>(ctx.gltfModel.textures.size())) {
		return nullptr;
	}

	int imageIndex = ctx.gltfModel.textures[textureIndex].source;
	if (imageIndex < 0 || imageIndex >= static_cast<int>(ctx.gltfModel.images.size())) {
		return nullptr;
	}

	const auto& img = ctx.gltfModel.images[imageIndex];

	if (!img.uri.empty() && ctx.resourceManager) {
		// External file - resolve relative to model directory
		std::string texPath = ctx.baseDir.empty() ? img.uri : ctx.baseDir + "/" + img.uri;
		LogSystem::get().trace("{}: [External file] {}", texName, texPath);
		return ctx.resourceManager->getTexture(texPath, format);
	} else if (img.bufferView >= 0 && ctx.resourceManager) {
		// Embedded: load from glTF buffer
		const auto& bufferView = ctx.gltfModel.bufferViews[img.bufferView];
		const auto& buffer = ctx.gltfModel.buffers[bufferView.buffer];
		const unsigned char* data = buffer.data.data() + bufferView.byteOffset;
		size_t size = bufferView.byteLength;

		std::string cacheKey = ctx.filepath + "_tex_" + std::to_string(textureIndex);
		LogSystem::get().trace("{}: [Embedded buffer] {} bytes", texName, size);
		return ctx.resourceManager->loadTextureFromMemory(data, size, cacheKey, format);
	} else if (!img.image.empty() && ctx.resourceManager) {
		// Image data loaded by tinygltf (decoded in memory)
		std::string cacheKey = ctx.filepath + "_tex_" + std::to_string(textureIndex);
		LogSystem::get().trace("{}: [Decoded image] {} bytes", texName, img.image.size());
		return ctx.resourceManager->loadTextureFromMemory(img.image.data(), img.image.size(), cacheKey, format);
	}

	return nullptr;
}

// Helper: set texture from path name
static void setMaterialTexture(std::shared_ptr<Material>& mat, const std::string& path, std::shared_ptr<Texture> tex) {
	if (path == "pbr.baseColorTexture")
		mat->baseColorMap = tex;
	else if (path == "pbr.metallicRoughnessTexture")
		mat->metallicRoughnessMap = tex;
	else if (path == "normalTexture")
		mat->normalMap = tex;
	else if (path == "occlusionTexture")
		mat->occlusionMap = tex;
	else if (path == "emissiveTexture")
		mat->emissiveMap = tex;
}

// Load material from glTF model
std::shared_ptr<Material> ModelLoader::loadGltfMaterial(const LoadContext& ctx, int materialIndex) {
	if (materialIndex < 0 || materialIndex >= static_cast<int>(ctx.gltfModel.materials.size())) {
		return nullptr;
	}

	const auto& gltfMat = ctx.gltfModel.materials[materialIndex];
	const auto& pbr = gltfMat.pbrMetallicRoughness;
	auto material = std::make_shared<Material>();

	// PBR Factors (Metallic Roughness)
	material->params.baseColorFactor = glm::make_vec4(pbr.baseColorFactor.data());
	material->params.metallicFactor = static_cast<float>(pbr.metallicFactor);
	material->params.roughnessFactor = static_cast<float>(pbr.roughnessFactor);

	// Emissive
	material->params.emissiveFactor = glm::make_vec3(gltfMat.emissiveFactor.data());

	// Alpha
	if (gltfMat.alphaMode == "MASK") {
		material->params.alphaMode = Material::PBRParameters::AlphaMode::MASK;
	} else if (gltfMat.alphaMode == "BLEND") {
		material->params.alphaMode = Material::PBRParameters::AlphaMode::BLEND;
	} else {
		material->params.alphaMode = Material::PBRParameters::AlphaMode::OPAQUE_MODE;
	}
	material->params.alphaCutoff = static_cast<float>(gltfMat.alphaCutoff);
	material->params.doubleSided = gltfMat.doubleSided;

	// Normal Scale & Occlusion Strength
	material->params.normalScale = static_cast<float>(gltfMat.normalTexture.scale);
	material->params.occlusionStrength = static_cast<float>(gltfMat.occlusionTexture.strength);

	// Standard texture paths
	struct TextureDef {
		const char* name;
		int index;
		vk::Format format;
	};
	std::vector<TextureDef> textureDefs = {
		{"pbr.baseColorTexture", pbr.baseColorTexture.index, vk::Format::eR8G8B8A8Srgb},
		{"pbr.metallicRoughnessTexture", pbr.metallicRoughnessTexture.index, vk::Format::eR8G8B8A8Unorm},
		{"normalTexture", gltfMat.normalTexture.index, vk::Format::eR8G8B8A8Unorm},
		{"occlusionTexture", gltfMat.occlusionTexture.index, vk::Format::eR8G8B8A8Unorm},
		{"emissiveTexture", gltfMat.emissiveTexture.index, vk::Format::eR8G8B8A8Srgb},
	};

	std::vector<std::string> loadedTextureNames;
	for (const auto& texDef : textureDefs) {
		if (texDef.index >= 0) {
			auto tex = loadGltfTexture(ctx, texDef.index, texDef.format, texDef.name);
			setMaterialTexture(material, texDef.name, tex);
			if (tex) {
				loadedTextureNames.emplace_back(texDef.name);
			}
		}
	}

	// Process extension textures
	for (const auto& [extName, extValue] : gltfMat.extensions) {
		if (extName == GltfExtensions::CLEARCOAT) {
			std::string prefix = "KHR_materials_clearcoat.";
			std::vector<std::pair<std::string, vk::Format>> clearcoatTextures = {
				{"clearcoatTexture", vk::Format::eR8G8B8A8Unorm},
				{"clearcoatRoughnessTexture", vk::Format::eR8G8B8A8Unorm},
				{"clearcoatNormalTexture", vk::Format::eR8G8B8A8Unorm},
			};

			for (const auto& [texName, format] : clearcoatTextures) {
				if (extValue.Has(texName)) {
					int texIdx = extValue.Get(texName).Get("index").GetNumberAsInt();
					std::string fullName = prefix + texName;
					auto tex = loadGltfTexture(ctx, texIdx, format, fullName);

					if (texName == "clearcoatTexture")
						material->clearcoatMap = tex;
					else if (texName == "clearcoatRoughnessTexture")
						material->clearcoatRoughnessMap = tex;
					else if (texName == "clearcoatNormalTexture")
						material->clearcoatNormalMap = tex;

					if (tex)
						loadedTextureNames.push_back(fullName);
				}
			}

			if (extValue.Has("clearcoatFactor"))
				material->params.clearcoatFactor =
					static_cast<float>(extValue.Get("clearcoatFactor").GetNumberAsDouble());
			if (extValue.Has("clearcoatRoughnessFactor"))
				material->params.clearcoatRoughnessFactor =
					static_cast<float>(extValue.Get("clearcoatRoughnessFactor").GetNumberAsDouble());

		} else if (extName == GltfExtensions::TRANSMISSION) {
			std::string prefix = std::string(GltfExtensions::TRANSMISSION) + ".";
			if (extValue.Has("transmissionTexture")) {
				int texIdx = extValue.Get("transmissionTexture").Get("index").GetNumberAsInt();
				auto tex = loadGltfTexture(ctx, texIdx, vk::Format::eR8G8B8A8Unorm, prefix + "transmissionTexture");
				material->transmissionMap = tex;
				if (tex)
					loadedTextureNames.push_back(prefix + "transmissionTexture");
			}
			if (extValue.Has("transmissionFactor"))
				material->params.transmissionFactor =
					static_cast<float>(extValue.Get("transmissionFactor").GetNumberAsDouble());

		} else if (extName == GltfExtensions::IOR) {
			if (extValue.Has("ior"))
				material->params.ior = static_cast<float>(extValue.Get("ior").GetNumberAsDouble());
		} else if (extName == GltfExtensions::EMISSIVE_STRENGTH) {
			if (extValue.Has("emissiveStrength"))
				material->params.emissiveStrength =
					static_cast<float>(extValue.Get("emissiveStrength").GetNumberAsDouble());
		}
	}

	// Log loaded textures
	if (!loadedTextureNames.empty()) {
		LogSystem::get().info("Material textures loaded: [{}]", joinStrings(loadedTextureNames));
	} else {
		LogSystem::get().info("Material textures: none");
	}

	return material;
}

// Process a glTF node recursively, extracting meshes and materials
void ModelLoader::processGltfNode(const LoadContext& ctx, int nodeIndex, std::shared_ptr<Model>& model) {
	const auto& node = ctx.gltfModel.nodes[nodeIndex];

	if (node.mesh >= 0) {
		const auto& gltfMesh = ctx.gltfModel.meshes[node.mesh];

		for (const auto& primitive : gltfMesh.primitives) {
			std::vector<Vertex> vertices;
			std::vector<uint32_t> indices;

			// Indices
			if (primitive.indices >= 0) {
				const auto& accessor = ctx.gltfModel.accessors[primitive.indices];
				const auto& bufferView = ctx.gltfModel.bufferViews[accessor.bufferView];
				const auto& buffer = ctx.gltfModel.buffers[bufferView.buffer];

				const uint8_t* dataPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
				size_t count = accessor.count;
				indices.reserve(count);

				if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(dataPtr);
					for (size_t i = 0; i < count; ++i)
						indices.push_back(buf[i]);
				} else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
					const uint32_t* buf = reinterpret_cast<const uint32_t*>(dataPtr);
					for (size_t i = 0; i < count; ++i)
						indices.push_back(buf[i]);
				} else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
					const uint8_t* buf = dataPtr;
					for (size_t i = 0; i < count; ++i)
						indices.push_back(buf[i]);
				}
			}

			// Attributes - dynamically collect all attributes from glTF
			std::map<std::string, std::pair<const float*, int>> attributeBuffers;
			std::vector<std::string> attributeNames;
			size_t vertexCount = 0;

			for (const auto& [attrName, accessorIdx] : primitive.attributes) {
				const auto& accessor = ctx.gltfModel.accessors[accessorIdx];
				const auto& bufferView = ctx.gltfModel.bufferViews[accessor.bufferView];
				const auto& buffer = ctx.gltfModel.buffers[bufferView.buffer];

				const float* dataPtr =
					reinterpret_cast<const float*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
				int stride = accessor.ByteStride(bufferView) ? accessor.ByteStride(bufferView) / sizeof(float)
															 : tinygltf::GetNumComponentsInType(accessor.type);

				attributeBuffers[attrName] = {dataPtr, stride};
				attributeNames.push_back(attrName);

				if (vertexCount == 0)
					vertexCount = accessor.count;
			}

			// Log all found attributes
			{
				bool hasTangent = attributeBuffers.contains("TANGENT");
				LogSystem::get().info("Mesh attributes: [{}]{}", joinStrings(attributeNames),
									  hasTangent ? "" : " (TANGENT will be computed)");
			}

			// Build vertices from collected attributes
			vertices.reserve(vertexCount);
			for (size_t i = 0; i < vertexCount; ++i) {
				Vertex v{};

				if (auto it = attributeBuffers.find("POSITION"); it != attributeBuffers.end()) {
					v.pos = glm::make_vec3(&it->second.first[i * it->second.second]);
				}
				if (auto it = attributeBuffers.find("NORMAL"); it != attributeBuffers.end()) {
					v.normal = glm::make_vec3(&it->second.first[i * it->second.second]);
				}
				if (auto it = attributeBuffers.find("TEXCOORD_0"); it != attributeBuffers.end()) {
					v.texCoord = glm::make_vec2(&it->second.first[i * it->second.second]);
				}
				if (auto it = attributeBuffers.find("TANGENT"); it != attributeBuffers.end()) {
					v.tangent = glm::make_vec4(&it->second.first[i * it->second.second]);
				}

				vertices.push_back(v);
			}

			// Compute tangents on CPU if not provided by the model
			bool hasTangent = attributeBuffers.contains("TANGENT");
			if (!hasTangent && !indices.empty()) {
				computeTangents(vertices, indices);
			}

			auto mesh = std::make_shared<Mesh>(vertices, indices);
			auto material = loadGltfMaterial(ctx, primitive.material);

			model->addPrimitive({mesh, material});
		}
	}

	// Recursively process children
	for (int child : node.children) {
		processGltfNode(ctx, child, model);
	}
}

std::shared_ptr<Model> ModelLoader::loadModel(const std::string& filepath, ResourceManager* resourceManager) {
	tinygltf::Model gltfModel;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = false;
	if (filepath.ends_with(".glb")) {
		ret = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filepath);
	} else {
		ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filepath);
	}

	if (!warn.empty()) {
		LogSystem::get().info("TinyGLTF Warning: {}", warn);
	}

	if (!err.empty()) {
		LogSystem::get().error("TinyGLTF Error: {}", err);
	}

	if (!ret) {
		LogSystem::get().error("Failed to parse glTF: {}", filepath);
		return nullptr;
	}

	// Log extensions for debugging
	logGltfExtensions(gltfModel);

	auto model = std::make_shared<Model>();

	// Get base directory for resolving relative texture paths
	std::filesystem::path modelPath(filepath);
	std::string baseDir = modelPath.parent_path().string();

	// Create context for helper functions
	LoadContext ctx{gltfModel, filepath, baseDir, resourceManager};

	// Process scene nodes using extracted function
	const auto& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];
	for (int nodeIndex : scene.nodes) {
		processGltfNode(ctx, nodeIndex, model);
	}

	LogSystem::get().info("Loaded model: {} with {} primitives", filepath, model->getPrimitives().size());

	return model;
}

bool ModelLoader::loadModelIntoScene(const std::string& filepath, Scene& scene, ResourceManager* resourceManager) {
	// Load model using existing function
	auto model = loadModel(filepath, resourceManager);
	if (!model) {
		return false;
	}

	// Convert each primitive to an entity
	for (const auto& primitive : model->getPrimitives()) {
		// Create entity (automatically gets TagComponent and TransformComponent)
		Entity entity = scene.createEntity("MeshEntity");

		// Add mesh component
		if (primitive.mesh) {
			entity.addComponent<MeshComponent>(primitive.mesh);
		}

		// Add mesh renderer component
		if (primitive.material) {
			entity.addComponent<MeshRendererComponent>(primitive.material);
		} else {
			entity.addComponent<MeshRendererComponent>();
		}
	}

	LogSystem::get().info("Loaded {} entities into scene from: {}", model->getPrimitives().size(), filepath);
	return true;
}

} // namespace Fishy
