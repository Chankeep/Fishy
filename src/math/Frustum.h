#pragma once
#include <array>
#include <glm/glm.hpp>

namespace Fishy::Math {

enum class FrustumPlaneIndex : size_t { Left = 0, Right = 1, Top = 2, Buttom = 3, Near = 4, Far = 5 };

void extractFrustumPlanes(const glm::mat4& viewProj, std::array<glm::vec4, 6>& planes,
						  bool normalizePlanes = true, bool vulkanDepthRange = true);

} // namespace Fishy::Math