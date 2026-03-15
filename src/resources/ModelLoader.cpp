#include "ModelLoader.h"
#include "ResourceManager.h"
#include "core/Result.h"
#include "ecs/Entity.h"
#include "ecs/components/HierarchyComponent.h"
#include "ecs/components/MeshComponent.h"
#include "ecs/components/MeshRendererComponent.h"
#include "ecs/components/TagComponent.h"
#include "ecs/components/TransformComponent.h"
#include "entt/core/fwd.hpp"
#include "entt/resource/resource.hpp"
#include "fastgltf/math.hpp"
#include "fastgltf/util.hpp"
#include "scene/Scene.h"

#include <expected>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/util.hpp>

namespace Fishy {

// Core includes and aliases
using namespace std::literals;

// Context struct to hold shared state between helper functions
struct ModelLoader::LoadContext {
	const fastgltf::Asset& gltfModel;
	const std::string& filepath;
	const std::string& baseDir;
	ResourceManager& resourceManager;
	uint32_t nextMeshId;	 // Counter for unique mesh IDs
	uint32_t nextMaterialId; // Counter for unique material IDs
	Scene* targetScene = nullptr;
	std::vector<Entity> createdEntities;
};

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
static void logGltfExtensions(const fastgltf::Asset& gltfModel) {
	if (!gltfModel.extensionsUsed.empty()) {
		FISHY_LOG_INFO("glTF extensions used:");
		for (const auto& ext : gltfModel.extensionsUsed) {
			FISHY_LOG_INFO("  - {}", ext);
		}
	}

	if (!gltfModel.extensionsRequired.empty()) {
		FISHY_LOG_INFO("glTF extensions required:");
		for (const auto& ext : gltfModel.extensionsRequired) {
			std::string extInfo = "  - " + std::string(ext);
			if (ext == fastgltf::extensions::KHR_materials_clearcoat ||
				ext == fastgltf::extensions::KHR_materials_transmission ||
				ext == fastgltf::extensions::KHR_materials_ior ||
				ext == fastgltf::extensions::KHR_materials_volume ||
				ext == fastgltf::extensions::KHR_materials_sheen ||
				ext == fastgltf::extensions::KHR_materials_specular ||
				ext == fastgltf::extensions::KHR_materials_emissive_strength) {
				extInfo += " (material extension - partial support)";
			} else if (ext == fastgltf::extensions::KHR_texture_basisu) {
				extInfo += " (requires KTX2/Basis Universal - NOT YET SUPPORTED)";
			} else if (ext == fastgltf::extensions::KHR_draco_mesh_compression) {
				extInfo += " (requires Draco - NOT SUPPORTED)";
			} else if (ext == fastgltf::extensions::KHR_mesh_quantization) {
				extInfo += " (mesh quantization - supported via fastgltf)";
			} else {
				extInfo += " (UNKNOWN)";
			}
			FISHY_LOG_INFO("{}", extInfo);
		}
	}
}

// Compute tangents for vertices when they are not provided by the model
static void computeTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
	std::vector<glm::vec3> tan1(vertices.size(), glm::vec3(0.0f));
	std::vector<glm::vec3> tan2(vertices.size(), glm::vec3(0.0f));

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

		glm::vec3 e1 = v1 - v0;
		glm::vec3 e2 = v2 - v0;

		glm::vec2 duv1 = uv1 - uv0;
		glm::vec2 duv2 = uv2 - uv0;

		float r = 1.0f / (duv1.x * duv2.y - duv2.x * duv1.y + 1e-6f);

		glm::vec3 tangent = (e1 * duv2.y - e2 * duv1.y) * r;
		glm::vec3 bitangent = (e2 * duv1.x - e1 * duv2.x) * r;

		tan1[i0] += tangent;
		tan1[i1] += tangent;
		tan1[i2] += tangent;

		tan2[i0] += bitangent;
		tan2[i1] += bitangent;
		tan2[i2] += bitangent;
	}

	for (size_t i = 0; i < vertices.size(); ++i) {
		const glm::vec3& n = vertices[i].normal;
		const glm::vec3& t = tan1[i];

		glm::vec3 tangent = glm::normalize(t - n * glm::dot(n, t));
		float w = (glm::dot(glm::cross(n, t), tan2[i]) < 0.0f) ? -1.0f : 1.0f;

		vertices[i].tangent = glm::vec4(tangent, w);
	}

	FISHY_LOG_TRACE("Computed {} vertex tangents", vertices.size());
}

// Generate unique cache ID for textures
static entt::id_type makeTextureId(const std::string& filepath, int textureIndex, vk::Format format) {
	std::string key = filepath + "_tex_" + std::to_string(textureIndex) + "_fmt_" +
					  std::to_string(static_cast<int>(format));
	return entt::hashed_string{key.c_str()};
}

// Generate unique cache ID for meshes
static entt::id_type makeMeshId(const std::string& filepath, uint32_t meshIndex) {
	std::string key = filepath + "_mesh_" + std::to_string(meshIndex);
	return entt::hashed_string{key.c_str()};
}

// Generate unique cache ID for materials
static entt::id_type makeMaterialId(const std::string& filepath, size_t materialIndex) {
	std::string key = filepath + "_mat_" + std::to_string(materialIndex);
	return entt::hashed_string{key.c_str()};
}

// Load texture from glTF model
entt::resource<Texture> ModelLoader::loadGltfTexture(const LoadContext& ctx, size_t textureIndex,
													 vk::Format format, const std::string& texName) {
	if (textureIndex >= ctx.gltfModel.textures.size()) {
		return {};
	}

	const auto& texture = ctx.gltfModel.textures[textureIndex];
	if (!texture.imageIndex.has_value()) {
		return {};
	}

	size_t imageIndex = texture.imageIndex.value();
	if (imageIndex >= ctx.gltfModel.images.size()) {
		return {};
	}

	const auto& img = ctx.gltfModel.images[imageIndex];

	return std::visit(
		fastgltf::visitor{
			[&](const fastgltf::sources::BufferView& bvSource) -> entt::resource<Texture> {
				const auto& bufferView = ctx.gltfModel.bufferViews[bvSource.bufferViewIndex];
				const auto& buffer = ctx.gltfModel.buffers[bufferView.bufferIndex];

				return std::visit(
					fastgltf::visitor{[&](const auto& source) -> entt::resource<Texture> {
						if constexpr (requires { source.bytes; }) {
							auto data = std::span<const std::byte>(
								source.bytes.data() + bufferView.byteOffset, bufferView.byteLength);

							entt::id_type id = makeTextureId(ctx.filepath, textureIndex, format);
							FISHY_LOG_TRACE("{}: [Embedded buffer] {} bytes", texName, data.size());
							return ctx.resourceManager.loadTextureFromMemory(id, data, format);
						} else {
							FISHY_LOG_WARN("{}: Unsupported buffer data source", texName);
							return {};
						}
					}},
					buffer.data);
			},

			[&](const fastgltf::sources::URI& uriSource) -> entt::resource<Texture> {
				auto path = uriSource.uri.path();
				std::string texPath =
					ctx.baseDir.empty() ? std::string(path) : ctx.baseDir + "/" + std::string(path);
				FISHY_LOG_TRACE("{}: [External file] {}", texName, texPath);
				entt::id_type id = entt::hashed_string{texPath.c_str()};
				return ctx.resourceManager.loadTexture(id, texPath, format);
			},
			[&](const auto&) -> entt::resource<Texture> {
				FISHY_LOG_WARN("{}: Unsupported image data source", texName);
				return {};
			}},
		img.data);
}

// Helper: set texture on material
static void setMaterialTexture(Material& mat, const std::string& path, entt::resource<Texture> tex) {
	if (path == "pbr.baseColorTexture")
		mat.baseColorMap = std::move(tex);
	else if (path == "pbr.metallicRoughnessTexture")
		mat.metallicRoughnessMap = std::move(tex);
	else if (path == "normalTexture")
		mat.normalMap = std::move(tex);
	else if (path == "occlusionTexture")
		mat.occlusionMap = std::move(tex);
	else if (path == "emissiveTexture")
		mat.emissiveMap = std::move(tex);
	// Extension textures
	else if (path == "KHR_materials_clearcoat.clearcoatTexture")
		mat.clearcoatMap = std::move(tex);
	else if (path == "KHR_materials_clearcoat.clearcoatRoughnessTexture")
		mat.clearcoatRoughnessMap = std::move(tex);
	else if (path == "KHR_materials_clearcoat.clearcoatNormalTexture")
		mat.clearcoatNormalMap = std::move(tex);
	else if (path == "KHR_materials_transmission.transmissionTexture")
		mat.transmissionMap = std::move(tex);
}
// Apply glTF node transform to TransformComponent (TRS-first, matrix fallback)
void ModelLoader::applyNodeTransform(const fastgltf::Node& node, TransformComponent& transform) {
	if (auto* trs = std::get_if<fastgltf::TRS>(&node.transform)) {
		transform.position = glm::make_vec3(trs->translation.data());
		// fastgltf quaternion is [x, y, z, w], GLM is (w, x, y, z)
		transform.rotation =
			glm::quat(trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2]);
		transform.scale = glm::make_vec3(trs->scale.data());
	} else if (auto* mat = std::get_if<fastgltf::math::fmat4x4>(&node.transform)) {
		glm::mat4 m = glm::make_mat4(mat->data());
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(m, transform.scale, transform.rotation, transform.position, skew, perspective);
	}
	transform.dirty = true;
}

// Load material from glTF model
entt::resource<Material> ModelLoader::loadGltfMaterial(const LoadContext& ctx, size_t materialIndex) {
	if (materialIndex >= ctx.gltfModel.materials.size()) {
		return {};
	}

	entt::id_type materialId = makeMaterialId(ctx.filepath, materialIndex);

	// Check if already cached
	auto existing = ctx.resourceManager.getMaterial(materialId);
	if (existing) {
		return existing;
	}

	const auto& gltfMat = ctx.gltfModel.materials[materialIndex];
	const auto& pbr = gltfMat.pbrData;

	// Create material in cache
	auto materialHandle = ctx.resourceManager.createMaterial(materialId);
	Material& material = *materialHandle;

	// === PBR Factors (Metallic Roughness) ===
	material.params.baseColorFactor = glm::make_vec4(pbr.baseColorFactor.data());
	material.params.metallicFactor = pbr.metallicFactor;
	material.params.roughnessFactor = pbr.roughnessFactor;

	// === Emissive ===
	material.params.emissiveFactor = glm::make_vec3(gltfMat.emissiveFactor.data());

	// === Alpha ===
	switch (gltfMat.alphaMode) {
	case fastgltf::AlphaMode::Mask:
		material.params.alphaMode = Material::PBRParameters::AlphaMode::MASK;
		break;
	case fastgltf::AlphaMode::Blend:
		material.params.alphaMode = Material::PBRParameters::AlphaMode::BLEND;
		break;
	default:
		material.params.alphaMode = Material::PBRParameters::AlphaMode::OPAQUE_MODE;
		break;
	}
	material.params.alphaCutoff = gltfMat.alphaCutoff;
	material.params.doubleSided = gltfMat.doubleSided;

	// === Normal Scale & Occlusion Strength ===
	material.params.normalScale = gltfMat.normalTexture.has_value() ? gltfMat.normalTexture->scale : 1.0f;
	material.params.occlusionStrength =
		gltfMat.occlusionTexture.has_value() ? gltfMat.occlusionTexture->strength : 1.0f;

	// === Standard texture paths ===
	std::vector<std::string> loadedTextureNames;

	auto tryLoadTexture = [&](const auto& texInfo, vk::Format format, const char* name) {
		if (texInfo.has_value()) {
			auto tex = loadGltfTexture(ctx, texInfo->textureIndex, format, name);
			setMaterialTexture(material, name, std::move(tex));
			loadedTextureNames.emplace_back(name);
		}
	};

	tryLoadTexture(pbr.baseColorTexture, vk::Format::eR8G8B8A8Srgb, "pbr.baseColorTexture");
	tryLoadTexture(pbr.metallicRoughnessTexture, vk::Format::eR8G8B8A8Unorm, "pbr.metallicRoughnessTexture");
	tryLoadTexture(gltfMat.normalTexture, vk::Format::eR8G8B8A8Unorm, "normalTexture");
	tryLoadTexture(gltfMat.occlusionTexture, vk::Format::eR8G8B8A8Unorm, "occlusionTexture");
	tryLoadTexture(gltfMat.emissiveTexture, vk::Format::eR8G8B8A8Srgb, "emissiveTexture");

	// === Extension: Clearcoat ===
	if (gltfMat.clearcoat) {
		material.params.clearcoatFactor = gltfMat.clearcoat->clearcoatFactor;
		material.params.clearcoatRoughnessFactor = gltfMat.clearcoat->clearcoatRoughnessFactor;

		tryLoadTexture(gltfMat.clearcoat->clearcoatTexture, vk::Format::eR8G8B8A8Unorm,
					   "KHR_materials_clearcoat.clearcoatTexture");
		tryLoadTexture(gltfMat.clearcoat->clearcoatRoughnessTexture, vk::Format::eR8G8B8A8Unorm,
					   "KHR_materials_clearcoat.clearcoatRoughnessTexture");
		tryLoadTexture(gltfMat.clearcoat->clearcoatNormalTexture, vk::Format::eR8G8B8A8Unorm,
					   "KHR_materials_clearcoat.clearcoatNormalTexture");
	}

	// === Extension: Transmission ===
	if (gltfMat.transmission) {
		material.params.transmissionFactor = gltfMat.transmission->transmissionFactor;
		tryLoadTexture(gltfMat.transmission->transmissionTexture, vk::Format::eR8G8B8A8Unorm,
					   "KHR_materials_transmission.transmissionTexture");
	}

	// === Extension: IOR & Emissive Strength ===
	material.params.ior = gltfMat.ior;
	material.params.emissiveStrength = gltfMat.emissiveStrength;

	// Log loaded textures
	if (!loadedTextureNames.empty()) {
		FISHY_LOG_INFO("Material textures loaded: [{}]", joinStrings(loadedTextureNames));
	} else {
		FISHY_LOG_TRACE("Material textures: none");
	}

	// Pre-compute bindless texture indices for this material
	ctx.resourceManager.populateMaterialTextureIndices(material);

	return materialHandle;
}

// Process a glTF node recursively, creating entities directly in the scene
void ModelLoader::processGltfNode(LoadContext& ctx, size_t nodeIndex, Entity parentEntity) {
	const auto& node = ctx.gltfModel.nodes[nodeIndex];

	// Track the entity created for this node (for passing to children)
	Entity currentNodeEntity{};

	if (node.meshIndex.has_value()) {
		const auto& gltfMesh = ctx.gltfModel.meshes[node.meshIndex.value()];

		for (size_t primitiveIdx = 0; primitiveIdx < gltfMesh.primitives.size(); ++primitiveIdx) {
			// Build entity name (conditional for Debug/Release)
#ifndef NDEBUG
			std::string entityName = node.name.empty() ? std::format("Node_{}_{}", nodeIndex, primitiveIdx)
													   : std::string(node.name);
#else
			std::string entityName; // Empty string, SSO avoids heap allocation
#endif

			// Use wrapped Entity class
			Entity entity = ctx.targetScene->createEntity(entityName);
			ctx.createdEntities.push_back(entity);

			auto& transform = entity.getComponent<TransformComponent>();
			auto& hierarchy = entity.addComponent<HierarchyComponent>();
			applyNodeTransform(node, transform);

			// Set parent reference for hierarchy
			if (parentEntity.isValid()) {
				hierarchy.parent = parentEntity.getHandle();
			}

			const auto& primitive = gltfMesh.primitives[primitiveIdx];
			std::vector<Vertex> vertices;
			std::vector<uint32_t> indices;

			// Indices
			if (primitive.indicesAccessor.has_value()) {
				auto& accessor = ctx.gltfModel.accessors[primitive.indicesAccessor.value()];
				indices.resize(accessor.count);
				fastgltf::copyFromAccessor<uint32_t>(ctx.gltfModel, accessor, indices.data());
			}

			// Diagnostic logging of mesh attributes
			std::vector<std::string> attributeNames;
			for (const auto& attr : primitive.attributes) {
				attributeNames.push_back(std::string(attr.name));
			}
			FISHY_LOG_TRACE("{}: Found attributes: {}", entityName, joinStrings(attributeNames));

			// Build vertices from collected attributes
			auto* positionIt = primitive.findAttribute("POSITION");
			size_t vertexCount = positionIt != primitive.attributes.end()
									 ? ctx.gltfModel.accessors[positionIt->accessorIndex].count
									 : 0;
			vertices.resize(vertexCount);

			// POSITION
			if (positionIt != primitive.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec3>(
					ctx.gltfModel, ctx.gltfModel.accessors[positionIt->accessorIndex],
					[&](glm::vec3 pos, size_t idx) { vertices[idx].pos = pos; });
			}

			// NORMAL
			if (auto* it = primitive.findAttribute("NORMAL"); it != primitive.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec3>(
					ctx.gltfModel, ctx.gltfModel.accessors[it->accessorIndex],
					[&](glm::vec3 n, size_t idx) { vertices[idx].normal = n; });
			}

			// TEXCOORD_0
			if (auto* it = primitive.findAttribute("TEXCOORD_0"); it != primitive.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec2>(
					ctx.gltfModel, ctx.gltfModel.accessors[it->accessorIndex],
					[&](glm::vec2 uv, size_t idx) { vertices[idx].texCoord = uv; });
			}

			// TANGENT
			bool hasTangent = false;
			if (auto* it = primitive.findAttribute("TANGENT"); it != primitive.attributes.end()) {
				hasTangent = true;
				fastgltf::iterateAccessorWithIndex<glm::vec4>(
					ctx.gltfModel, ctx.gltfModel.accessors[it->accessorIndex],
					[&](glm::vec4 t, size_t idx) { vertices[idx].tangent = t; });
			}

			// Compute tangents on CPU if not provided by the model
			if (!hasTangent && !indices.empty()) {
				computeTangents(vertices, indices);
			}

			// Create mesh in cache with unique ID
			uint32_t meshIdCounter = static_cast<uint32_t>(node.meshIndex.value() * 1000 + primitiveIdx);
			entt::id_type meshId = makeMeshId(ctx.filepath, meshIdCounter);
			auto meshHandle = ctx.resourceManager.createMesh(meshId, vertices, indices);

			// Load material
			entt::resource<Material> materialHandle;
			if (primitive.materialIndex.has_value()) {
				materialHandle = loadGltfMaterial(ctx, primitive.materialIndex.value());
			}

			// Add components to entity (using wrapped Entity API)
			if (meshHandle) {
				entity.addComponent<MeshComponent>(meshHandle);
			}
			if (materialHandle) {
				entity.addComponent<MeshRendererComponent>(materialHandle);
			} else {
				entity.addComponent<MeshRendererComponent>();
			}

			// Use first primitive's entity as parent for children nodes
			if (primitiveIdx == 0) {
				currentNodeEntity = entity;
			}
		}
	} else if (!node.children.empty()) {
		// Node has no mesh but has children - create a pure transform entity
#ifndef NDEBUG
		std::string entityName =
			node.name.empty() ? std::format("TransformNode_{}", nodeIndex) : std::string(node.name);
#else
		std::string entityName;
#endif
		Entity entity = ctx.targetScene->createEntity(entityName);
		ctx.createdEntities.push_back(entity);

		auto& transform = entity.getComponent<TransformComponent>();
		auto& hierarchy = entity.addComponent<HierarchyComponent>();
		applyNodeTransform(node, transform);

		if (parentEntity.isValid()) {
			hierarchy.parent = parentEntity.getHandle();
		}

		currentNodeEntity = entity;
	}

	// Recursively process children, passing current entity as parent
	for (size_t child : node.children) {
		processGltfNode(ctx, child, currentNodeEntity);
	}
}

Result<std::vector<Entity>> ModelLoader::loadModelIntoScene(const std::string& filepath, Scene& scene,
															ResourceManager& resourceManager) {
	// Log if model was already loaded (entities will be created again, but resources are cached)
	if (resourceManager.isModelLoaded(filepath)) {
		FISHY_LOG_INFO("Creating additional instance of model: {}", filepath);
	}

	fastgltf::Parser parser(
		fastgltf::Extensions::KHR_materials_clearcoat | fastgltf::Extensions::KHR_materials_transmission |
		fastgltf::Extensions::KHR_materials_ior | fastgltf::Extensions::KHR_materials_emissive_strength |
		fastgltf::Extensions::KHR_mesh_quantization);

	auto data = fastgltf::GltfDataBuffer::FromPath(filepath);
	if (data.error() != fastgltf::Error::None) {
		return std::unexpected(
			ModelLoadError::make(ModelLoadError::Code::ParseFailed, "Failed to read file: " + filepath));
	}

	auto asset = parser.loadGltf(data.get(), std::filesystem::path(filepath).parent_path(),
								 fastgltf::Options::LoadExternalBuffers);
	if (asset.error() != fastgltf::Error::None) {
		return std::unexpected(
			ModelLoadError::make(ModelLoadError::Code::ParseFailed, "Failed to parse glTF: " + filepath));
	}

	// Log extensions for debugging
	logGltfExtensions(asset.get());

	// Get base directory
	std::string baseDir = std::filesystem::path(filepath).parent_path().string();

	// Create context with scene pointer
	LoadContext ctx{asset.get(), filepath, baseDir, resourceManager, 0, 0, &scene};

	// Process scene nodes directly into entities
	size_t sceneIndex = asset->defaultScene.has_value() ? asset->defaultScene.value() : 0;
	auto& gltfScene = asset->scenes[sceneIndex];
	for (size_t nodeIndex : gltfScene.nodeIndices) {
		processGltfNode(ctx, nodeIndex);
	}

	// Mark model as loaded to avoid redundant parsing
	resourceManager.markModelLoaded(filepath);

	FISHY_LOG_INFO("Loaded model into scene from: {}", filepath);
	return ctx.createdEntities;
}

} // namespace Fishy
