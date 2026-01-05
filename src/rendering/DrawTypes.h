#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.hpp>

namespace Fishy {

// Forward declarations
struct GPUMaterial;

// Invalid texture index sentinel
constexpr uint32_t INVALID_TEXTURE_INDEX = UINT32_MAX;

// Per-instance data stored in bindless SSBO for GPU access
// Layout must match shader InstanceData struct (std430)
struct alignas(16) InstanceData {
	// Transform data
	glm::mat4 model;		// Per-object model matrix (world transform)
	glm::mat4 normalMatrix; // transpose(inverse(mat3(model))) for correct normals

	// Bindless texture indices (UINT32_MAX = no texture, use default)
	uint32_t baseColorIndex;		  // Albedo map
	uint32_t metallicRoughnessIndex;  // MetallicRoughness packed
	uint32_t normalIndex;			  // Normal map
	uint32_t occlusionIndex;		  // Ambient occlusion
	uint32_t emissiveIndex;			  // Emissive map
	uint32_t clearcoatIndex;		  // Clearcoat map
	uint32_t clearcoatRoughnessIndex; // Clearcoat roughness
	uint32_t clearcoatNormalIndex;	  // Clearcoat normal
	uint32_t transmissionIndex;		  // Transmission map
	uint32_t _padding0[3];			  // Align to 16 bytes for vec4 baseColorFactor

	// Material properties (inline to avoid extra UBO lookup)
	glm::vec4 baseColorFactor; // RGBA base color multiplier
	float metallicFactor;
	float roughnessFactor;
	float normalScale;
	float occlusionStrength;
	glm::vec4 emissiveFactor; // RGB + strength
	float alphaCutoff;
	uint32_t flags; // Material flags (doubleSided, alphaMode, etc.)
	float clearcoatFactor;
	float clearcoatRoughnessFactor;

	// Compute InstanceData from a model matrix with default values
	[[nodiscard]] static InstanceData fromModelMatrix(const glm::mat4& modelMat) {
		InstanceData data{};
		static const glm::mat4 gltfCorrection =
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		data.model = gltfCorrection * modelMat;
		data.normalMatrix = glm::mat4(glm::inverseTranspose(glm::mat3(data.model)));

		// Default texture indices (invalid = use shader defaults)
		data.baseColorIndex = INVALID_TEXTURE_INDEX;
		data.metallicRoughnessIndex = INVALID_TEXTURE_INDEX;
		data.normalIndex = INVALID_TEXTURE_INDEX;
		data.occlusionIndex = INVALID_TEXTURE_INDEX;
		data.emissiveIndex = INVALID_TEXTURE_INDEX;
		data.clearcoatIndex = INVALID_TEXTURE_INDEX;
		data.clearcoatRoughnessIndex = INVALID_TEXTURE_INDEX;
		data.clearcoatNormalIndex = INVALID_TEXTURE_INDEX;
		data.transmissionIndex = INVALID_TEXTURE_INDEX;

		// Default material properties
		data.baseColorFactor = glm::vec4(1.0f);
		data.metallicFactor = 1.0f;
		data.roughnessFactor = 1.0f;
		data.normalScale = 1.0f;
		data.occlusionStrength = 1.0f;
		data.emissiveFactor = glm::vec4(0.0f);
		data.alphaCutoff = 0.5f;
		data.flags = 0;
		data.clearcoatFactor = 0.0f;
		data.clearcoatRoughnessFactor = 0.0f;

		return data;
	}
};

// Keep ObjectData as alias for backward compatibility
using ObjectData = InstanceData;

// Region of a mesh within unified vertex/index buffers
struct MeshRegion {
	uint32_t firstIndex;  // Offset into unified index buffer
	uint32_t indexCount;  // Number of indices for this mesh
	int32_t vertexOffset; // Offset into unified vertex buffer (added to indices)
};

// A batch of draw commands sharing the same material
struct DrawBatch {
	GPUMaterial* material; // Material for this batch (deprecated - will be removed)
	uint32_t firstCommand; // Offset into indirect buffer (command index)
	uint32_t commandCount; // Number of draw commands in this batch
};

} // namespace Fishy
