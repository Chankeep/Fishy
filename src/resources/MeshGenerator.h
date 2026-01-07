#pragma once

#include "Mesh.h" // 复用现有的 Vertex 结构

#include <memory>

namespace Fishy {

/**
 * @brief 用于生成基础几何体网格的工具类。
 */
class MeshGenerator {
public:
	/**
	 * @brief 生成一个单位立方体网格 (边长 2, 中心在原点)。
	 * @return 包含顶点和索引数据的 Mesh 对象。
	 *
	 * 用于 Skybox 渲染：顶点位置同时作为采样 Cubemap 的方向向量。
	 */
	[[nodiscard]] static std::shared_ptr<Mesh> createCube();
};

} // namespace Fishy