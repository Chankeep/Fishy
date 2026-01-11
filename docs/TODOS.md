# Fishy Engine - TODOs

## 📋 Pending TODOs

### 🏗️ Architecture (核心架构)

* [ ] **创建 `RenderContext` (RHI 层)**
  * [ ] 创建 `core/RenderContext.h/cpp`
  * [ ] 移入 `VulkanDevice`, `SwapChain`, `Surface` 的所有权
  * [ ] 实现 `FrameContext& beginFrame()` 和 `void endFrame(FrameContext&)`

* [ ] **创建 `GeometrySystem` (ECS System)**
  * [ ] 移动 `buildInstanceDataFromScene` 逻辑到 `GeometrySystem::update()`
  * [ ] 使用 `registry.group` 确保内存连续性
  * [ ] 实现 Dirty Flag 优化

* [ ] **GPU 数据上传优化**
  * [ ] `GeometrySystem` 输出 `std::span<InstanceData>` 和 `std::span<DrawCommand>`
  * [ ] 利用 `FrameContext` 的 `LinearAllocator` 上传数据

---

### 🌅 Shadow Mapping (阴影)

* [ ] **添加 Cascaded Shadow Maps (CSM)**
  * [ ] 实现多级阴影贴图
  * [ ] 根据相机距离选择合适的级联
  * [ ] 平滑过渡各级联边界

* [ ] **PBR 阴影效果调试**
  * [ ] 调整 shadow bias 参数减少 shadow acne
  * [ ] 优化 PCF 采样核提升软阴影质量
  * [ ] 阴影边缘抗锯齿

---

### ⚡ Performance (性能)

* [ ] **加速模型加载**
  * [ ] 并行纹理加载（多线程解码）
  * [ ] 模型二进制缓存（避免重复解析 glTF）
  * [ ] 异步资源加载管线

* [ ] **Pipeline 管理优化**
  * [ ] 创建 `PipelineBuilder` 缓存机制（Pipeline Library）
  * [ ] 在 Pass `init` 时按需创建 Pipeline

* [ ] **(Optional) Compute Shader Culling**
  * [ ] 插入 `CullPass` 使用 Compute Shader 生成 Indirect Draw Commands

---

### 🔧 Code Quality (代码质量)

* [ ] **引入 `vk::StructureChain`**
  * [ ] 检查所有 `pNext` 链的构建，改用 C++20 `vk::StructureChain`

---

## ✅ Archived (已完成)

### 2025-01-11
- [x] **Hierarchical Transform System** - 层级变换系统，支持 glTF 父子节点
- [x] **Frustum-Based Shadow Mapping** - 基于视锥体的阴影映射
- [x] **重构 `Renderer` 为 `RenderGraphExecutor`**
- [x] **拆分现有渲染逻辑** - MainDrawPass, SkyboxPass, UIPass
- [x] **定义 `IRenderPass` 接口**
- [x] **提取 `FrameContext`** - 每帧资源管理

### 2025-01-10
- [x] **封装 VMA Image (RAII Wrapper)** - VulkanImage 类
- [x] **优化 Bindless 纹理索引查找** - 预计算索引
- [x] **移除 SwapChain 重建时的 `DeviceWaitIdle`**

### 2025-01-03
- [x] **Bindless Descriptors** - Descriptor Indexing
- [x] **Vertex/Index Buffer Batching** - 统一缓冲区
- [x] **Per-Object Model Matrix with Bindless SSBO**
- [x] **Material Sorting / Batching**
- [x] **Proper Inverse Normal Matrix**
- [x] **Image-Based Lighting (IBL)**

### 2025-01-01
- [x] **Fix render loop heap allocations**
- [x] **Implement Pipeline Cache**
- [x] **Instanced/Indirect Rendering**
- [x] **Fix PBR material texture formats**
- [x] **Fix shader alphaRoughness calculation**
- [x] **Optimize Descriptor Set Binding**

### 2024-12-27
- [x] **Logging System** - spdlog integration