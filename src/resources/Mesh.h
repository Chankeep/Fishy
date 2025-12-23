#pragma once

#include <array>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace Fishy {

// Vertex structure matching glTF attributes
struct Vertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 texCoord;
	glm::vec4 tangent; // W component is handedness (+1 or -1)

	static vk::VertexInputBindingDescription getBindingDescription() {
		return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
	}

	static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions() {
		return {vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)},
				vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)},
				vk::VertexInputAttributeDescription{2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)},
				vk::VertexInputAttributeDescription{3, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, tangent)}};
	}
};

// Global uniform buffer (camera, lighting)
struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	alignas(16) glm::vec3 camPos;
	alignas(16) glm::vec3 lightDir;
	alignas(16) glm::vec3 lightColor;
};

// Mesh class - pure data container for geometry
class Mesh {
public:
	Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
	~Mesh() = default;

	const std::vector<Vertex>& getVertices() const { return _vertices; }
	const std::vector<uint32_t>& getIndices() const { return _indices; }

private:
	std::vector<Vertex> _vertices;
	std::vector<uint32_t> _indices;
};

} // namespace Fishy
