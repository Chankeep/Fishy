#pragma once

#include <array>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "math/AABB.h"

namespace Fishy {

// Vertex structure matching glTF attributes
struct Vertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 texCoord;
	glm::vec4 tangent; // W component is handedness (+1 or -1)

	[[nodiscard]] static vk::VertexInputBindingDescription getBindingDescription() {
		return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
	}

	[[nodiscard]] static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions() {
		return {vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)},
				vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)},
				vk::VertexInputAttributeDescription{2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)},
				vk::VertexInputAttributeDescription{3, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, tangent)}};
	}
};

// Global uniform buffer (camera, debug, IBL)
// Light and shadow data are in separate SSBOs, accessed via BDA
struct UniformBufferObject {
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec4 camPos;
	uint32_t lightCount;
	// Debug visualization controls
	float debugViewInputs = 0.0f;	// 0=off, 1=baseColor, 2=normal, 3=AO, 4=emissive, 5=metallic, 6=roughness
	float debugViewEquation = 0.0f; // 0=off, 1=diffuse, 2=F, 3=G, 4=D, 5=specular
	// IBL parameters
	float prefilteredMipLevels = 1.0f;
	float iblIntensity = 1.0f;
	float _pad[3];
};

// Mesh class - pure data container for geometry
class Mesh {
public:
	Mesh(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices);
	~Mesh() = default;

	[[nodiscard]] const std::vector<Vertex>& getVertices() const { return _vertices; }
	[[nodiscard]] const std::vector<uint32_t>& getIndices() const { return _indices; }
	[[nodiscard]] size_t getVertexCount() const { return _vertices.size(); }
	[[nodiscard]] size_t getIndexCount() const { return _indices.size(); }
	[[nodiscard]] const Math::AABB& getLocalAABB() const { return _localAABB; }

private:
	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices;
	Math::AABB _localAABB;
};

} // namespace Fishy
