#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <vulkan/vulkan.hpp>

namespace Fishy {

// Forward declarations
struct GPUMaterial;

// =============================================================================
// Per-Object Data (SSBO layout)
// =============================================================================

/// Per-object data stored in SSBO for GPU access
/// Layout must match shader ObjectData struct (std430)
struct alignas(16) ObjectData {
	glm::mat4 model;				   ///< Per-object model matrix (world transform)
	glm::mat4 normalMatrix;			   ///< transpose(inverse(mat3(model))) for correct normals
	uint32_t materialIndex;			   ///< Index into material array (future bindless)
	std::array<uint32_t, 3> padding{}; ///< Align to 16 bytes

	/// Compute ObjectData from a model matrix
	[[nodiscard]] static ObjectData fromModelMatrix(const glm::mat4& modelMat, uint32_t matIndex = 0) {
		ObjectData data{};
		data.model = modelMat;
		// Compute normalMatrix: transpose(inverse(mat3(model)))
		// glm::inverseTranspose handles this correctly
		data.normalMatrix = glm::mat4(glm::inverseTranspose(glm::mat3(modelMat)));
		data.materialIndex = matIndex;
		return data;
	}
};

// =============================================================================
// Draw Batching
// =============================================================================

/// Region of a mesh within unified vertex/index buffers
struct MeshRegion {
	uint32_t firstIndex;  ///< Offset into unified index buffer
	uint32_t indexCount;  ///< Number of indices for this mesh
	int32_t vertexOffset; ///< Offset into unified vertex buffer (added to indices)
};

/// A batch of draw commands sharing the same material
/// Uses unified buffers - no per-batch VB/IB binding needed
struct DrawBatch {
	GPUMaterial* material; ///< Material for this batch
	uint32_t firstCommand; ///< Offset into indirect buffer (command index)
	uint32_t commandCount; ///< Number of draw commands in this batch
};

} // namespace Fishy
