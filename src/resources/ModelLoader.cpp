#include "ModelLoader.h"
#include "ResourceManager.h"
#include "core/LogSystem.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// STB_IMAGE_IMPLEMENTATION is defined in Texture.cpp

#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <tiny_gltf.h>

namespace Fishy {

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

	// Log glTF extensions used
	if (!gltfModel.extensionsUsed.empty()) {
		LogSystem::get().info("glTF extensions used:");
		for (const auto& ext : gltfModel.extensionsUsed) {
			LogSystem::get().info("  - {}", ext);
		}
	}

	// Log required extensions and warn about unsupported ones
	if (!gltfModel.extensionsRequired.empty()) {
		LogSystem::get().info("glTF extensions required:");
		for (const auto& ext : gltfModel.extensionsRequired) {
			std::string extInfo = "  - " + ext;
			// Check if extension is supported
			if (ext == "KHR_materials_clearcoat" || ext == "KHR_materials_transmission" || ext == "KHR_materials_ior" ||
				ext == "KHR_materials_volume" || ext == "KHR_materials_sheen" || ext == "KHR_materials_specular" ||
				ext == "KHR_materials_emissive_strength") {
				extInfo += " (material extension - partial support)";
			} else if (ext == "KHR_texture_basisu") {
				extInfo += " (requires KTX2/Basis Universal - NOT YET SUPPORTED)";
			} else if (ext == "KHR_draco_mesh_compression") {
				extInfo += " (requires Draco - NOT SUPPORTED)";
			} else if (ext == "KHR_mesh_quantization") {
				extInfo += " (mesh quantization - supported via tinygltf)";
			} else {
				extInfo += " (UNKNOWN)";
			}
			LogSystem::get().info("{}", extInfo);
		}
	}

	auto model = std::make_shared<Model>();

	// Get base directory for resolving relative texture paths
	std::filesystem::path modelPath(filepath);
	std::string baseDir = modelPath.parent_path().string();

	// Helper to load texture (handles both external files and embedded textures)
	// format should be eR8G8B8A8Srgb for color textures, eR8G8B8A8Unorm for data textures
	auto loadTexture = [&](int textureIndex, vk::Format format = vk::Format::eR8G8B8A8Srgb,
						   const std::string& texName = "texture") -> std::shared_ptr<Texture> {
		if (textureIndex < 0 || textureIndex >= static_cast<int>(gltfModel.textures.size())) {
			return nullptr;
		}

		int imageIndex = gltfModel.textures[textureIndex].source;
		if (imageIndex < 0 || imageIndex >= static_cast<int>(gltfModel.images.size())) {
			return nullptr;
		}

		const auto& img = gltfModel.images[imageIndex];

		if (!img.uri.empty() && resourceManager) {
			// External file - resolve relative to model directory
			std::string texPath;
			if (baseDir.empty()) {
				texPath = img.uri;
			} else {
				texPath = baseDir + "/" + img.uri;
			}
			LogSystem::get().trace("{}: [External file] {}", texName, texPath);
			return resourceManager->getTexture(texPath, format);
		} else if (img.bufferView >= 0 && resourceManager) {
			// Embedded: load from glTF buffer
			const auto& bufferView = gltfModel.bufferViews[img.bufferView];
			const auto& buffer = gltfModel.buffers[bufferView.buffer];
			const unsigned char* data = buffer.data.data() + bufferView.byteOffset;
			size_t size = bufferView.byteLength;

			std::string cacheKey = filepath + "_tex_" + std::to_string(textureIndex);
			LogSystem::get().trace("{}: [Embedded buffer] {} bytes", texName, size);
			return resourceManager->loadTextureFromMemory(data, size, cacheKey, format);
		} else if (!img.image.empty() && resourceManager) {
			// Image data loaded by tinygltf (decoded in memory)
			// This happens when tinygltf decodes embedded base64 images
			std::string cacheKey = filepath + "_tex_" + std::to_string(textureIndex);
			LogSystem::get().trace("{}: [Decoded image] {} bytes", texName, img.image.size());
			return resourceManager->loadTextureFromMemory(img.image.data(), img.image.size(), cacheKey, format);
		}

		return nullptr;
	};

	// Texture path definition: name -> (getter function, format)
	using TextureGetter = std::function<int(const tinygltf::Material&)>;
	struct TexturePath {
		std::string name;
		TextureGetter getIndex;
		vk::Format format;
	};

	// Helper to load Material
	auto loadMaterial = [&](int materialIndex) -> std::shared_ptr<Material> {
		if (materialIndex < 0 || materialIndex >= static_cast<int>(gltfModel.materials.size())) {
			return nullptr;
		}

		const auto& gltfMat = gltfModel.materials[materialIndex];
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

		// Define all standard glTF texture paths
		std::vector<TexturePath> texturePaths = {
			{"pbr.baseColorTexture",
			 [](const tinygltf::Material& m) { return m.pbrMetallicRoughness.baseColorTexture.index; },
			 vk::Format::eR8G8B8A8Srgb},
			{"pbr.metallicRoughnessTexture",
			 [](const tinygltf::Material& m) { return m.pbrMetallicRoughness.metallicRoughnessTexture.index; },
			 vk::Format::eR8G8B8A8Unorm},
			{"normalTexture", [](const tinygltf::Material& m) { return m.normalTexture.index; },
			 vk::Format::eR8G8B8A8Unorm},
			{"occlusionTexture", [](const tinygltf::Material& m) { return m.occlusionTexture.index; },
			 vk::Format::eR8G8B8A8Unorm},
			{"emissiveTexture", [](const tinygltf::Material& m) { return m.emissiveTexture.index; },
			 vk::Format::eR8G8B8A8Srgb},
		};

		// Helper: set texture from path name
		auto setMaterialTexture = [&](std::shared_ptr<Material> mat, const std::string& path,
									  std::shared_ptr<Texture> tex) {
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
		};

		// Dynamically load all standard textures
		std::vector<std::string> loadedTextureNames;
		for (const auto& texPath : texturePaths) {
			int texIdx = texPath.getIndex(gltfMat);
			if (texIdx >= 0) {
				auto tex = loadTexture(texIdx, texPath.format, texPath.name);
				setMaterialTexture(material, texPath.name, tex);
				if (tex) {
					loadedTextureNames.push_back(texPath.name);
				}
			}
		}

		// Dynamically process extension textures
		for (const auto& [extName, extValue] : gltfMat.extensions) {
			if (extName == "KHR_materials_clearcoat") {
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
						auto tex = loadTexture(texIdx, format, fullName);

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

				// Extract factors
				if (extValue.Has("clearcoatFactor"))
					material->params.clearcoatFactor =
						static_cast<float>(extValue.Get("clearcoatFactor").GetNumberAsDouble());
				if (extValue.Has("clearcoatRoughnessFactor"))
					material->params.clearcoatRoughnessFactor =
						static_cast<float>(extValue.Get("clearcoatRoughnessFactor").GetNumberAsDouble());

			} else if (extName == "KHR_materials_transmission") {
				std::string prefix = "KHR_materials_transmission.";

				if (extValue.Has("transmissionTexture")) {
					int texIdx = extValue.Get("transmissionTexture").Get("index").GetNumberAsInt();
					auto tex = loadTexture(texIdx, vk::Format::eR8G8B8A8Unorm, prefix + "transmissionTexture");
					material->transmissionMap = tex;
					if (tex)
						loadedTextureNames.push_back(prefix + "transmissionTexture");
				}
				if (extValue.Has("transmissionFactor"))
					material->params.transmissionFactor =
						static_cast<float>(extValue.Get("transmissionFactor").GetNumberAsDouble());

			} else if (extName == "KHR_materials_ior") {
				if (extValue.Has("ior"))
					material->params.ior = static_cast<float>(extValue.Get("ior").GetNumberAsDouble());
			} else if (extName == "KHR_materials_emissive_strength") {
				if (extValue.Has("emissiveStrength"))
					material->params.emissiveStrength =
						static_cast<float>(extValue.Get("emissiveStrength").GetNumberAsDouble());
			}
		}

		// Dynamic texture logging
		if (!loadedTextureNames.empty()) {
			std::string texList;
			for (size_t i = 0; i < loadedTextureNames.size(); ++i) {
				if (i > 0)
					texList += ", ";
				texList += loadedTextureNames[i];
			}
			LogSystem::get().info("Material textures loaded: [{}]", texList);
		} else {
			LogSystem::get().info("Material textures: none");
		}

		return material;
	};

	// Helper to load Meshes
	const auto& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];

	// Helper recursive function to process nodes
	std::function<void(int)> processNode = [&](int nodeIndex) {
		const auto& node = gltfModel.nodes[nodeIndex];

		if (node.mesh >= 0) {
			const auto& gltfMesh = gltfModel.meshes[node.mesh];

			for (const auto& primitive : gltfMesh.primitives) {
				std::vector<Vertex> vertices;
				std::vector<uint32_t> indices;

				// Indices
				if (primitive.indices >= 0) {
					const auto& accessor = gltfModel.accessors[primitive.indices];
					const auto& bufferView = gltfModel.bufferViews[accessor.bufferView];
					const auto& buffer = gltfModel.buffers[bufferView.buffer];

					const uint8_t* dataPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
					size_t count = accessor.count;

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
					const auto& accessor = gltfModel.accessors[accessorIdx];
					const auto& bufferView = gltfModel.bufferViews[accessor.bufferView];
					const auto& buffer = gltfModel.buffers[bufferView.buffer];

					const float* dataPtr = reinterpret_cast<const float*>(buffer.data.data() + bufferView.byteOffset +
																		  accessor.byteOffset);
					int stride = accessor.ByteStride(bufferView) ? accessor.ByteStride(bufferView) / sizeof(float)
																 : tinygltf::GetNumComponentsInType(accessor.type);

					attributeBuffers[attrName] = {dataPtr, stride};
					attributeNames.push_back(attrName);

					if (vertexCount == 0)
						vertexCount = accessor.count;
				}

				// Log all found attributes
				{
					std::string attrsStr;
					for (size_t i = 0; i < attributeNames.size(); ++i) {
						if (i > 0)
							attrsStr += ", ";
						attrsStr += attributeNames[i];
					}
					bool hasTangent = attributeBuffers.count("TANGENT") > 0;
					LogSystem::get().info("Mesh attributes: [{}]{}", attrsStr,
										  hasTangent ? "" : " (TANGENT will be computed)");
				}

				// Build vertices from collected attributes
				vertices.reserve(vertexCount);
				for (size_t i = 0; i < vertexCount; ++i) {
					Vertex v{};

					// Get known attributes from dynamic map
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
					// Tangent will be computed after vertex loop if not provided

					vertices.push_back(v);
				}

				// Compute tangents on CPU if not provided by the model
				bool hasTangent = attributeBuffers.count("TANGENT") > 0;
				if (!hasTangent && !indices.empty()) {
					computeTangents(vertices, indices);
				}

				auto mesh = std::make_shared<Mesh>(vertices, indices);
				auto material = loadMaterial(primitive.material);

				model->addPrimitive({mesh, material});
			}
		}

		for (int child : node.children) {
			processNode(child);
		}
	};

	for (int nodeIndex : scene.nodes) {
		processNode(nodeIndex);
	}

	LogSystem::get().info("Loaded model: {} with {} primitives", filepath, model->getPrimitives().size());

	return model;
}

} // namespace Fishy
