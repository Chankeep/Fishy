# **现代 Vulkan 渲染引擎 MVP 1.0 \- 需求规格说明书 (PRD)**

**版本:** 1.3.0

**状态:** 活跃

**更新日期:** 2025/12/30 (添加 ECS 架构需求 REQ-E-01 ~ REQ-E-07)

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
- [x] **REQ-C-05 (日志系统):** 集成 `spdlog` 实现统一日志管理。
  * 支持多输出目标：控制台（彩色）、文件、循环缓冲区（用于 ImGui 显示）
  * 支持日志级别：trace, info, warn, error, critical
  * 使用 `std::source_location` 自动记录日志产生的文件名、行号和函数名
  * 提供 Vulkan Debug Callback 集成，接收 Validation Layers 调试信息

### **2.2 渲染管线 (Render Pipeline)**

- [x] **REQ-R-01 (前向渲染 Pass):** 实现一个标准的 Forward Render Pass。  
  * 包含深度测试 (Depth Test) 和 深度写入 (Depth Write)。  
  * 清除值配置：Color (黑色), Depth (1.0)。  
- [x] **REQ-R-02 (动态状态):** 使用 Dynamic State (Viewport, Scissor) 以避免因窗口 Resize 导致的管线重建。  
- [x] **REQ-R-03 (Shader 管理):** 支持 Vertex 和 Fragment Shader 的加载。
  * 输入：SPIR-V 字节码。
- [ ] **REQ-R-04 (Shader Reflection):** 从 SPIR-V 自动提取管线配置。
  * 解析 vertex input attributes（location, format, offset）
  * 解析 descriptor bindings（set, binding, type, stage）
  * 解析 push constant ranges
  * 按照约定验证 shader/Material 兼容性
  * 缓存反射结果避免重复解析
- [ ] **REQ-R-05 (管线缓存):** 实现基础的 Pipeline 缓存机制（例如 HashMap），避免相同材质配置重复创建 Pipeline Object。

### **2.3 资源管理 (Resource Management)**

- [x] **REQ-M-00 (VMA 内存管理):** 集成 Vulkan Memory Allocator (VMA) 实现自动化显存管理。
  * 在 `VulkanDevice` 中创建和管理 `VmaAllocator` 生命周期
  * `VulkanBuffer` 使用 `vmaCreateBuffer()` 分配缓冲区内存
  * `Texture` 使用 `vmaCreateImage()` 分配图像内存
  * 支持多种内存类型：HostVisible、DeviceLocal
  * 提供内存映射和 Staging Buffer 上传机制
- [x] **REQ-M-01 (Buffer 封装):** 封装 VulkanBuffer 类。  
  * 支持 Staging Buffer 上传机制（Host \-\> Device Local）。  
- [x] **REQ-M-02 (纹理系统):** \* 加载 .jpg/.png/.ktx2 图片。
  * **KTX2 支持**: 支持加载 KTX2 格式纹理，利用 Basis Universal 压缩减少显存占用。
  * 支持 vk::raii::Sampler 创建（线性过滤，Repeat 模式）。  
  * **必须实现**：图像布局转换 (Layout Transition) 的 Command 录制。  
- [ ] **REQ-M-03 (描述符管理):** \* 实现动态 Descriptor Set Allocator。  
  * 避免手动管理单个 Set 的释放，支持按帧重置 Pool。  
- [ ] **REQ-M-04 (资源哈希):** 使用路径哈希 ID (Size\_t/Uint64) 替代字符串作为资源的运行时唯一标识。

### **2.4 场景与对象 (Scene \& Objects)**

- [ ] **REQ-S-01 (模型加载):** 集成 `tinygltf` / `tinyobjloader` 支持运行时加载 .gltf/.glb/.obj 模型。  
  * 解析节点层级、Mesh、Primitive。
  * **内嵌数据支持**: 支持读取 .gltf 文件中内嵌的 Material 和 Texture (Embedded Buffers/Images)。
  * **支持运行时导入**: 通过路径字符串动态加载并生成 Mesh 资源。
- [ ] **REQ-S-02 (数据导向变换):** * **TransformSystem**: 使用 std::vector\<glm::mat4\> 连续存储所有物体的世界矩阵。  
  * 场景节点仅存储索引 ID。  
- [ ] **REQ-S-03 (基础剔除):** 实现 CPU 端视锥体剔除 (Frustum Culling)，基于 AABB 包围盒，输出可见物体索引列表。

### **2.5 材质与光照 (Material \& Lighting)**

- [x] **REQ-L-01 (PBR 材质数据):**  
  * 支持 Albedo, Metallic, Roughness, Normal 贴图。  
  * 通过 Descriptor Set 绑定至 Shader。  
- [ ] **REQ-L-02 (glTF PBR支持):** 实现符合 glTF 2.0 标准的 Metallic-Roughness 工作流 Shader。
  * **属性支持**: 读取 glTF material 中的 `baseColorFactor`, `metallicFactor`, `roughnessFactor` 等参数并通过 SSBO 传递。
  * **纹理支持**: 采样 `baseColorTexture`, `metallicRoughnessTexture`, `normalTexture`, `occlusionTexture`。
  * **内嵌数据**: 支持直接从 glTF BufferView 创建纹理对象用于 Shader 采样。
- [ ] **REQ-L-03 (全局光照数据):** \* 实现一个全局 Scene UBO。  
  * 包含：相机矩阵 (View, Proj)、环境光参数、主方向光参数。  
  * 支持点光源数组 (Point Lights)。

### **2.6 交互 (Interaction)**

- [x] **REQ-I-01 (漫游相机):** 实现轨道视察相机 (Orbit Camera) 用于模型检查。
  * 鼠标右键拖拽: 围绕目标旋转
  * 鼠标滚轮: 缩放
  * 鼠标中键拖拽: 平移
  * 自动移除模型的时间自动旋转
- [ ] **REQ-I-02 (编辑器 UI):** 集成 Dear ImGui (Docking Branch)。
  * **Scene Hierarchy**: 显示场景物体列表，点击选中。
  * **Inspector**: 实时修改选中物体的 Transform 和材质。
  * **Asset Browser**: 浏览并加载磁盘上的模型与纹理。
  * **Material Creator**: 创建新材质并分配纹理资源。
- [x] **REQ-I-03 (输入屏蔽):** 当操作 UI 时，屏蔽相机的鼠标输入。

### **2.7 性能优化 (Performance Optimization)**

- [ ] **REQ-P-01 (Pipeline Cache):** 实现 VkPipelineCache 持久化机制。
  * 启动时从磁盘加载缓存数据
  * 退出时保存缓存到磁盘
  * 避免重复编译相同配置的管线

- [ ] **REQ-P-02 (Transfer Queue 异步上传):** 使用专用 Transfer Queue 进行资源上传。
  * 避免阻塞图形队列
  * 支持异步资源加载
  * 使用信号量同步传输完成

- [ ] **REQ-P-03 (Mipmap 生成):** 为纹理自动生成 Mipmap。
  * 使用 `vkCmdBlitImage` 逐级生成
  * 或支持加载预生成 Mipmap 的纹理 (KTX2)
  * 配置合适的 Sampler minFilter/mipmapMode

- [ ] **REQ-P-04 (Instanced/Indirect Rendering):** 减少 Draw Call 开销。
  * 相同材质的物体使用 Instanced Rendering 合并
  * 实现 `vkCmdDrawIndexedIndirect` 支持
  * 预留 GPU-Driven 扩展接口

- [ ] **REQ-P-05 (Bindless 描述符):** 实现 Bindless/Descriptor Indexing 架构。
  * 使用纹理数组 + 索引替代多次绑定
  * 减少描述符集切换开销
  * 支持动态纹理数量

- [ ] **REQ-P-06 (零分配渲染循环):** 消除渲染循环中的堆分配。
  * 使用 `std::array` 替代 `std::vector` 存储临时数据
  * 预分配所有帧级资源
  * 避免动态容器操作

### **2.8 ECS 架构 (Entity-Component-System)**

> 将 OOP 架构迁移到 ECS，提升数据局部性和扩展性

- [ ] **REQ-E-01 (World/Registry):** 实现 Entity-Component 管理器。
  * Entity ID 系统 (sparse set 或 generational index)
  * 高效的 Component 存储和查询
  * 考虑集成 [entt](https://github.com/skypjack/entt) 库

- [ ] **REQ-E-02 (Core Components):** 实现核心 Components。
  * `TransformComponent`: position, rotation, scale, worldMatrix, parent
  * `MeshComponent`: meshHandle, boundingBox
  * `MaterialComponent`: materialHandle, parameter overrides
  * `CameraComponent`: fov, nearPlane, farPlane, viewMatrix, projMatrix
  * `LightComponent`: type, color, intensity, range

- [ ] **REQ-E-03 (TransformSystem):** 层级变换计算系统。
  * 计算 parent-child 层级变换
  * 批量更新 dirty 的 worldMatrix
  * 上传变换数据到 GPU (SSBO)

- [ ] **REQ-E-04 (RenderSystem):** 渲染系统。
  * 查询具有 Transform + Mesh + Material 的 entities
  * 按材质排序减少 state changes
  * 录制绘制命令
  * Renderer 简化为底层 Vulkan 封装

- [ ] **REQ-E-05 (CullingSystem):** 剔除系统。
  * 视锥体剔除 (Frustum Culling)
  * 输出可见 entity 列表

- [ ] **REQ-E-06 (CameraSystem):** 相机系统。
  * 更新活动相机的 view/projection 矩阵
  * 处理相机输入控制

- [ ] **REQ-E-07 (LightSystem):** 灯光系统。
  * 收集场景中的所有灯光
  * 上传灯光数据到 GPU UBO/SSBO

## **3\. 非功能性需求 (NFR)**

- [ ] **NFR-01 (性能):** 在 GTX 1060 级别显卡上，渲染 500 个带纹理物体保持 60 FPS 稳定。  
- [x] **NFR-02 (同步正确性):** 0 验证层错误 (Validation Errors)。  
- [ ] **NFR-03 (代码风格):** 核心渲染循环零内存分配 (Zero allocation in render loop)。

## **4\. 范围排除 (Out of Scope for MVP 1.0)**

* 多线程任务调度 (Job System) \- 仅留接口，单线程执行。  
* GPU Driven Rendering (Mesh Shaders / GPU Culling)。  
* 阴影映射 (Shadow Mapping)。  
* 后期处理 (Post Processing)。