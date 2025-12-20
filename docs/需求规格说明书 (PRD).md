# **现代 Vulkan 渲染引擎 MVP 1.0 \- 需求规格说明书 (PRD)**

**版本:** 1.1

**状态:** 已冻结

**更新日期:** 2025/10/24 (基于架构优化建议修订)

## **1\. 项目概述**

构建一个基于 Vulkan 的现代渲染器原型。核心目标是验证**多帧并行渲染架构**、**模块化资源管理**以及**基础 PBR 渲染流程**的正确性，为后续的高级特性（GPU-Driven, Render Graph）打下坚实的地基。

## **2\. 功能性需求 (Functional Requirements)**

### **2.1 平台与核心架构 (Core \& Platform)**

- [x] **REQ-C-01 (窗口化):** 集成 GLFW，创建一个可调整大小的窗口，支持全屏切换和关闭事件。  
- [x] **REQ-C-02 (Vulkan 初始化):** 初始化 Vulkan Instance 和 Device。  
  * 必须开启 Validation Layers (在 Debug 模式下)。  
  * 集成 VK\_EXT\_debug\_utils，支持在 RenderDoc 中显示 Debug Marker 和 Label。  
- [x] **REQ-C-03 (交换链管理):** 实现 Swapchain 的创建与重建（处理窗口大小改变），支持双重缓冲（Double Buffering）或三重缓冲。  
- [x] **REQ-C-04 (多帧并行架构 \- Frames in Flight):** \* 核心需求：实现至少 2 帧的并发渲染。  
  * 为每一帧分配独立的 CommandPool、CommandBuffer、Semaphores (ImageAvailable, RenderFinished) 和 Fence。

### **2.2 渲染管线 (Render Pipeline)**

- [x] **REQ-R-01 (前向渲染 Pass):** 实现一个标准的 Forward Render Pass。  
  * 包含深度测试 (Depth Test) 和 深度写入 (Depth Write)。  
  * 清除值配置：Color (黑色), Depth (1.0)。  
- [x] **REQ-R-02 (动态状态):** 使用 Dynamic State (Viewport, Scissor) 以避免因窗口 Resize 导致的管线重建。  
- [x] **REQ-R-03 (Shader 管理):** 支持 Vertex 和 Fragment Shader 的加载。  
  * 输入：SPIR-V 字节码。  
- [ ] **REQ-R-04 (管线缓存):** 实现基础的 Pipeline 缓存机制（例如 HashMap），避免相同材质配置重复创建 Pipeline Object。

### **2.3 资源管理 (Resource Management)**

- [x] **REQ-M-01 (Buffer 封装):** 封装 VulkanBuffer 类。  
  * 支持 Staging Buffer 上传机制（Host \-\> Device Local）。  
- [x] **REQ-M-02 (纹理系统):** \* 加载 .jpg/.png 图片并生成 vk::raii::Image 和 vk::raii::ImageView。  
  * 支持 vk::raii::Sampler 创建（线性过滤，Repeat 模式）。  
  * **必须实现**：图像布局转换 (Layout Transition) 的 Command 录制。  
- [ ] **REQ-M-03 (描述符管理):** \* 实现动态 Descriptor Set Allocator。  
  * 避免手动管理单个 Set 的释放，支持按帧重置 Pool。  
- [ ] **REQ-M-04 (资源哈希):** 使用路径哈希 ID (Size\_t/Uint64) 替代字符串作为资源的运行时唯一标识。

### **2.4 场景与对象 (Scene \& Objects)**

- [ ] **REQ-S-01 (模型加载):** 集成 tinygltf 加载 .gltf/.glb 模型。  
  * 解析节点层级、Mesh、Primitive。  
- [ ] **REQ-S-02 (数据导向变换):** \* **TransformSystem**: 使用 std::vector\<glm::mat4\> 连续存储所有物体的世界矩阵。  
  * 场景节点仅存储索引 ID。  
- [ ] **REQ-S-03 (基础剔除):** 实现 CPU 端视锥体剔除 (Frustum Culling)，基于 AABB 包围盒，输出可见物体索引列表。

### **2.5 材质与光照 (Material \& Lighting)**

- [ ] **REQ-L-01 (PBR 材质数据):**  
  * 支持 Albedo, Metallic, Roughness, Normal 贴图。  
  * 通过 Descriptor Set 绑定至 Shader。  
- [ ] **REQ-L-02 (全局光照数据):** \* 实现一个全局 Scene UBO。  
  * 包含：相机矩阵 (View, Proj)、环境光参数、主方向光参数。  
  * 支持点光源数组 (Point Lights)。

### **2.6 交互 (Interaction)**

- [ ] **REQ-I-01 (漫游相机):** 实现第一人称 FPS 相机 (WASD 移动 \+ 鼠标右键旋转)。

## **3\. 非功能性需求 (NFR)**

- [ ] **NFR-01 (性能):** 在 GTX 1060 级别显卡上，渲染 500 个带纹理物体保持 60 FPS 稳定。  
- [x] **NFR-02 (同步正确性):** 0 验证层错误 (Validation Errors)。  
- [ ] **NFR-03 (代码风格):** 核心渲染循环零内存分配 (Zero allocation in render loop)。

## **4\. 范围排除 (Out of Scope for MVP 1.0)**

* 多线程任务调度 (Job System) \- 仅留接口，单线程执行。  
* 运行时热重载 (Hot Reloading)。  
* GPU Driven Rendering (Mesh Shaders / GPU Culling)。  
* 阴影映射 (Shadow Mapping)。  
* 后期处理 (Post Processing)。