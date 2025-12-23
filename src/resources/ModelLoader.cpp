#include "ModelLoader.h"
#include "ResourceManager.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// STB_IMAGE_IMPLEMENTATION is defined in Texture.cpp

#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <tiny_gltf.h>

namespace Fishy {

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
		std::cout << "TinyGLTF Warning: " << warn << std::endl;
	}

	if (!err.empty()) {
		std::cerr << "TinyGLTF Error: " << err << std::endl;
	}

	if (!ret) {
		std::cerr << "Failed to parse glTF: " << filepath << std::endl;
		return nullptr;
	}

	// Log glTF extensions used
	if (!gltfModel.extensionsUsed.empty()) {
		std::cout << "glTF extensions used:" << std::endl;
		for (const auto& ext : gltfModel.extensionsUsed) {
			std::cout << "  - " << ext << std::endl;
		}
	}

	// Log required extensions and warn about unsupported ones
	if (!gltfModel.extensionsRequired.empty()) {
		std::cout << "glTF extensions required:" << std::endl;
		for (const auto& ext : gltfModel.extensionsRequired) {
			std::cout << "  - " << ext;
			// Check if extension is supported
			if (ext == "KHR_materials_clearcoat" || ext == "KHR_materials_transmission" || ext == "KHR_materials_ior" ||
				ext == "KHR_materials_volume" || ext == "KHR_materials_sheen" || ext == "KHR_materials_specular" ||
				ext == "KHR_materials_emissive_strength") {
				std::cout << " (material extension - partial support)";
			} else if (ext == "KHR_texture_basisu") {
				std::cout << " (requires KTX2/Basis Universal - NOT YET SUPPORTED)";
			} else if (ext == "KHR_draco_mesh_compression") {
				std::cout << " (requires Draco - NOT SUPPORTED)";
			} else if (ext == "KHR_mesh_quantization") {
				std::cout << " (mesh quantization - supported via tinygltf)";
			} else {
				std::cout << " (UNKNOWN)";
			}
			std::cout << std::endl;
		}
	}

	auto model = std::make_shared<Model>();

	// Get base directory for resolving relative texture paths
	std::filesystem::path modelPath(filepath);
	std::string baseDir = modelPath.parent_path().string();

	// Helper to load texture (handles both external files and embedded textures)
	// format should be eR8G8B8A8Srgb for color textures, eR8G8B8A8Unorm for data textures
	auto loadTexture = [&](int textureIndex,
						   vk::Format format = vk::Format::eR8G8B8A8Srgb) -> std::shared_ptr<Texture> {
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
			return resourceManager->getTexture(texPath, format);
		} else if (img.bufferView >= 0 && resourceManager) {
			// Embedded: load from glTF buffer
			const auto& bufferView = gltfModel.bufferViews[img.bufferView];
			const auto& buffer = gltfModel.buffers[bufferView.buffer];
			const unsigned char* data = buffer.data.data() + bufferView.byteOffset;
			size_t size = bufferView.byteLength;

			std::string cacheKey = filepath + "_tex_" + std::to_string(textureIndex);
			return resourceManager->loadTextureFromMemory(data, size, cacheKey, format);
		} else if (!img.image.empty() && resourceManager) {
			// Image data loaded by tinygltf (decoded in memory)
			// This happens when tinygltf decodes embedded base64 images
			std::string cacheKey = filepath + "_tex_" + std::to_string(textureIndex);
			return resourceManager->loadTextureFromMemory(img.image.data(), img.image.size(), cacheKey, format);
		}

		return nullptr;
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

		// Core Textures - use appropriate formats:
		// sRGB for color data (baseColor, emissive)
		// UNORM for linear/data textures (normal, metallicRoughness, occlusion)
		material->baseColorMap = loadTexture(pbr.baseColorTexture.index, vk::Format::eR8G8B8A8Srgb);
		material->metallicRoughnessMap = loadTexture(pbr.metallicRoughnessTexture.index, vk::Format::eR8G8B8A8Srgb);
		material->normalMap = loadTexture(gltfMat.normalTexture.index, vk::Format::eR8G8B8A8Unorm);
		material->occlusionMap = loadTexture(gltfMat.occlusionTexture.index, vk::Format::eR8G8B8A8Srgb);
		material->emissiveMap = loadTexture(gltfMat.emissiveTexture.index, vk::Format::eR8G8B8A8Srgb);

		// Log texture presence
		std::cout << "  Material textures:" << std::endl;
		std::cout << "    baseColorMap:        " << (material->baseColorMap ? "loaded" : "not present") << std::endl;
		std::cout << "    metallicRoughnessMap:" << (material->metallicRoughnessMap ? "loaded" : "not present")
				  << std::endl;
		std::cout << "    normalMap:           " << (material->normalMap ? "loaded" : "not present") << std::endl;
		std::cout << "    occlusionMap:        " << (material->occlusionMap ? "loaded" : "not present") << std::endl;
		std::cout << "    emissiveMap:         " << (material->emissiveMap ? "loaded" : "not present") << std::endl;

		// Parse material extensions
		for (const auto& [extName, extValue] : gltfMat.extensions) {
			if (extName == "KHR_materials_clearcoat") {
				if (extValue.Has("clearcoatFactor")) {
					material->params.clearcoatFactor =
						static_cast<float>(extValue.Get("clearcoatFactor").GetNumberAsDouble());
				}
				if (extValue.Has("clearcoatRoughnessFactor")) {
					material->params.clearcoatRoughnessFactor =
						static_cast<float>(extValue.Get("clearcoatRoughnessFactor").GetNumberAsDouble());
				}
				if (extValue.Has("clearcoatTexture")) {
					int texIndex = extValue.Get("clearcoatTexture").Get("index").GetNumberAsInt();
					material->clearcoatMap = loadTexture(texIndex, vk::Format::eR8G8B8A8Srgb);
				}
				if (extValue.Has("clearcoatRoughnessTexture")) {
					int texIndex = extValue.Get("clearcoatRoughnessTexture").Get("index").GetNumberAsInt();
					material->clearcoatRoughnessMap = loadTexture(texIndex, vk::Format::eR8G8B8A8Srgb);
				}
				if (extValue.Has("clearcoatNormalTexture")) {
					int texIndex = extValue.Get("clearcoatNormalTexture").Get("index").GetNumberAsInt();
					material->clearcoatNormalMap = loadTexture(texIndex, vk::Format::eR8G8B8A8Srgb);
				}
				std::cout << "    KHR_materials_clearcoat: factor=" << material->params.clearcoatFactor
						  << ", clearcoatMap=" << (material->clearcoatMap ? "loaded" : "not present")
						  << ", roughnessMap=" << (material->clearcoatRoughnessMap ? "loaded" : "not present")
						  << ", normalMap=" << (material->clearcoatNormalMap ? "loaded" : "not present") << std::endl;
			} else if (extName == "KHR_materials_transmission") {
				if (extValue.Has("transmissionFactor")) {
					material->params.transmissionFactor =
						static_cast<float>(extValue.Get("transmissionFactor").GetNumberAsDouble());
				}
				if (extValue.Has("transmissionTexture")) {
					int texIndex = extValue.Get("transmissionTexture").Get("index").GetNumberAsInt();
					material->transmissionMap = loadTexture(texIndex, vk::Format::eR8G8B8A8Srgb);
				}
				std::cout << "    KHR_materials_transmission: factor=" << material->params.transmissionFactor
						  << ", transmissionMap=" << (material->transmissionMap ? "loaded" : "not present")
						  << std::endl;
			} else if (extName == "KHR_materials_ior") {
				if (extValue.Has("ior")) {
					material->params.ior = static_cast<float>(extValue.Get("ior").GetNumberAsDouble());
				}
				std::cout << "    KHR_materials_ior: ior=" << material->params.ior << std::endl;
			} else if (extName == "KHR_materials_emissive_strength") {
				if (extValue.Has("emissiveStrength")) {
					material->params.emissiveStrength =
						static_cast<float>(extValue.Get("emissiveStrength").GetNumberAsDouble());
				}
				std::cout << "    KHR_materials_emissive_strength: strength=" << material->params.emissiveStrength
						  << std::endl;
			}
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

				// Attributes
				const float* bufferPos = nullptr;
				const float* bufferNorm = nullptr;
				const float* bufferTex = nullptr;
				const float* bufferTan = nullptr;

				int stridePos = 0, strideNorm = 0, strideTex = 0, strideTan = 0;
				size_t vertexCount = 0;

				auto getAttributeBuffer = [&](const char* name, const float** outBuffer, int* outStride) {
					if (primitive.attributes.find(name) != primitive.attributes.end()) {
						const auto& accessor = gltfModel.accessors[primitive.attributes.at(name)];
						const auto& bufferView = gltfModel.bufferViews[accessor.bufferView];
						const auto& buffer = gltfModel.buffers[bufferView.buffer];
						*outBuffer = reinterpret_cast<const float*>(buffer.data.data() + bufferView.byteOffset +
																	accessor.byteOffset);
						*outStride = accessor.ByteStride(bufferView) ? accessor.ByteStride(bufferView) / sizeof(float)
																	 : tinygltf::GetNumComponentsInType(accessor.type);
						if (vertexCount == 0)
							vertexCount = accessor.count;
					}
				};

				getAttributeBuffer("POSITION", &bufferPos, &stridePos);
				getAttributeBuffer("NORMAL", &bufferNorm, &strideNorm);
				getAttributeBuffer("TEXCOORD_0", &bufferTex, &strideTex);
				getAttributeBuffer("TANGENT", &bufferTan, &strideTan);

				// Log attribute presence
				std::cout << "  Mesh primitive attributes:" << std::endl;
				std::cout << "    POSITION:   " << (bufferPos ? "present" : "MISSING") << std::endl;
				std::cout << "    NORMAL:     " << (bufferNorm ? "present" : "MISSING") << std::endl;
				std::cout << "    TEXCOORD_0: " << (bufferTex ? "present" : "MISSING") << std::endl;
				std::cout << "    TANGENT:    "
						  << (bufferTan ? "present (will use provided)" : "MISSING (will compute from derivatives)")
						  << std::endl;

				vertices.reserve(vertexCount);
				for (size_t i = 0; i < vertexCount; ++i) {
					Vertex v{};
					if (bufferPos)
						v.pos = glm::make_vec3(&bufferPos[i * stridePos]);
					if (bufferNorm)
						v.normal = glm::make_vec3(&bufferNorm[i * strideNorm]);
					if (bufferTex)
						v.texCoord = glm::make_vec2(&bufferTex[i * strideTex]);
					if (bufferTan)
						v.tangent = glm::make_vec4(&bufferTan[i * strideTan]);
					else
						v.tangent =
							glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // No tangent - shader will compute from derivatives
					vertices.push_back(v);
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

	std::cout << "Loaded model: " << filepath << " with " << model->getPrimitives().size() << " primitives"
			  << std::endl;

	return model;
}

} // namespace Fishy
