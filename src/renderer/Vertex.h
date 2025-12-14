#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS // 移除Vulkan.hpp的构造函数

#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace Fishy {

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	static vk::VertexInputBindingDescription getBindingDescription() {
		return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
	}

	static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
		return {vk::VertexInputAttributeDescription{0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos)},
				vk::VertexInputAttributeDescription{1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)}};
	}
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

} // namespace Fishy
