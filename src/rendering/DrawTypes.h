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
constexpr uint32_t INVALID_TEXTURE_INDEX = 0xFFFFFFFF;

// Per-instance data stored in bindless SSBO for GPU access
// Layout must match shader InstanceData struct (std430)
struct alignas(16) InstanceData {
	// Transform data
	glm::mat4 model; // Per-object model matrix (world transform)
					 // glm::mat4 normalMatrix; // transpose(inverse(mat3(model))) for correct normals

	// --- Material Factors (Packed into vec4s for alignment) ---

	// Data 0: Base Color (RGBA)
	glm::vec4 baseColorFactor;

	// Data 1: Emissive (RGB) + Strength (A)
	glm::vec4 emissiveFactor;

	// Data 2: PBR Standard
	// x: metallic, y: roughness, z: normalScale, w: occlusionStrength
	glm::vec4 pbrFactors;

	// Data 3: Advanced / Alpha
	// x: alphaCutoff, y: transmission, z: ior, w: clearcoatFactor
	glm::vec4 extraFactors1;

	// Data 4: Clearcoat details
	// x: clearcoatRoughness, y: unused, z: unused, w: unused
	glm::vec4 extraFactors2;

	// --- Bindless Indices (Integers) ---
	// Note: uints are 4 bytes. We group them to align to 16 bytes where possible
	// or just let them trail. std430 aligns arrays/structs, but simple uints are 4-byte aligned.
	uint32_t baseColorIndex;
	uint32_t metallicRoughnessIndex;
	uint32_t normalIndex;
	uint32_t occlusionIndex;

	uint32_t emissiveIndex;
	uint32_t clearcoatIndex;
	uint32_t clearcoatRoughnessIndex;
	uint32_t clearcoatNormalIndex;

	uint32_t transmissionIndex;
	uint32_t flags;	   // Bitfield for alphaMode, doubleSided
	uint32_t objectID; // Useful for mouse picking / debug
	uint32_t _pad0;	   // Align to 16 bytes end
};

// Ensure struct size is valid for GPU arrays
static_assert(sizeof(InstanceData) % 16 == 0, "InstanceData size must be multiple of 16 for std430");

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
