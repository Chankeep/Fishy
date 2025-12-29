# TODOS

## 🔴 High Priority

- [ ] **Fix render loop heap allocations (REQ-P-06)**
  - Replace `std::vector<vk::DescriptorSet>` with `std::array` in `Renderer::render()`
  - Eliminate all dynamic allocations in hot path
  - Related: NFR-03 (零分配渲染循环)

- [ ] **Implement Pipeline Cache (REQ-P-01, REQ-R-05)**
  - Add `vk::raii::PipelineCache` to `Renderer` or `ResourceManager`
  - Load cache from disk on startup
  - Save cache to disk on shutdown
  - Pass cache to `PipelineBuilder::build()`

- [ ] **Instanced/Indirect Rendering (REQ-P-04)**
  - Group primitives by material for instanced rendering
  - Implement `vkCmdDrawIndexedIndirect`
  - Reduce draw call overhead for 500+ objects (NFR-01)

- [ ] **Fix PBR material (texture formats, gamma)**
  - `metallicRoughnessTexture` should use `eR8G8B8A8Unorm` (data texture, not color)
  - Verify all data textures use correct formats
  - Ensure sRGB/linear conversion is correct

## 🟡 Medium Priority

- [ ] **Shader Reflection System (REQ-R-04)**
  - Integrate SPIRV-Reflect library
  - Add `ShaderReflection` struct for vertex inputs and descriptor bindings
  - Auto-generate `PipelineVertexInputStateCreateInfo` and `DescriptorSetLayout`
  - Validate shader/Material compatibility (glTF standard binding order)
  - Cache reflection results in `ResourceManager`

- [ ] **Transfer Queue Async Upload (REQ-P-02)**
  - Add dedicated transfer queue to `VulkanDevice`
  - Implement async resource upload with semaphore sync
  - Don't block graphics queue during texture/mesh loading

- [ ] **Mipmap Generation (REQ-P-03)**
  - Generate mipmaps using `vkCmdBlitImage`
  - Update Sampler with proper `mipmapMode` and `maxLod`
  - Enable anisotropic filtering

- [ ] **KTX2 Texture Support (REQ-M-02)**
  - Integrate libktx library
  - Support BC7/BC5 compressed textures
  - Load pre-generated mipmaps from KTX2

- [ ] **ImGui Integration and Input Handling (REQ-I-02)**
  - Complete ImGuiLayer implementation
  - Add Scene Hierarchy panel
  - Add Inspector panel
  - Add Asset Browser

## � Renderer Design Improvements

- [ ] **Material Sorting / Batching**
  - Sort primitives by material before rendering to reduce descriptor set switches
  - Group same-material objects together for potential instancing
  - Current: 每个 primitive 切换一次材质描述符

- [ ] **Global Descriptor Set 优化**
  - Global Set (Set 0) 只需绑定一次，不需要每个 primitive 都重新绑定
  - 当前代码每次循环都调用 `bindDescriptorSets` 绑定所有 sets
  - 修改: 循环外绑定 Set 0，循环内只绑定 Set 1 (Material)

- [ ] **Vertex/Index Buffer Batching**
  - 合并相同 Mesh 的顶点/索引数据到单个大 Buffer
  - 使用 `firstIndex` 和 `vertexOffset` 参数避免重复绑定
  - 当前: 每个 primitive 都调用 `bindVertexBuffers` 和 `bindIndexBuffer`

- [ ] **Per-Object Model Matrix (Push Constants / SSBO)**
  - 当前 Model 矩阵在 GlobalUBO 中，所有物体共享
  - 改为 Push Constants (小数据) 或 SSBO (大量物体)
  - 支持多物体不同变换

- [ ] **Depth Pre-Pass (可选)**
  - 先进行 Depth-Only 渲染，填充深度缓冲
  - 主 Pass 使用 `DepthCompareOp::eEqual` 避免 overdraw
  - 对复杂遮挡场景有性能提升

- [ ] **Multi-Pipeline Support**
  - 当前只有一个 GraphicsPipeline
  - 需要支持不同材质类型的 Pipeline (透明/不透明/双面)
  - 根据 Material::alphaMode 选择 Pipeline

- [ ] **Secondary Command Buffers (可选)**
  - 将场景渲染和 UI 渲染分离到 Secondary Command Buffers
  - 便于多线程录制
  - 当前: 单一 Primary Command Buffer

## �🟢 Low Priority / Future

- [ ] **Bindless Descriptors (REQ-P-05)**
  - Implement Descriptor Indexing extension
  - Use texture arrays with material indices
  - Reduce descriptor set switching overhead

- [ ] **Dynamic Descriptor Allocator (REQ-M-03)**
  - Implement growable descriptor pool
  - Support per-frame pool reset
  - Remove fixed pool size limits

- [ ] **Resource Hashing (REQ-M-04)**
  - Replace string keys with hash IDs (size_t/uint64_t)
  - Faster resource lookup

- [ ] **TransformSystem (REQ-S-02)**
  - Use `std::vector<glm::mat4>` for contiguous storage
  - Scene nodes store only index IDs
  - Batch matrix updates

- [ ] **Frustum Culling (REQ-S-03)**
  - Implement CPU AABB culling
  - Output visible object indices

## 🏗️ ECS Architecture (Future Refactor)

> 将当前的 OOP 架构迁移到 Entity-Component-System 架构，提升数据局部性和扩展性

### 架构层次图

```mermaid
graph TB
    subgraph "Application Layer"
        APP[Application]
    end
    
    subgraph "ECS Layer"
        WORLD[World/Registry]
        subgraph "Components"
            TC[TransformComp]
            MC[MeshComp]
            MATC[MaterialComp]
            CC[CameraComp]
            LC[LightComp]
        end
        subgraph "Systems"
            TS[TransformSystem]
            CS[CameraSystem]
            LS[LightSystem]
            CULLS[CullingSystem]
            RS[RenderSystem]
        end
    end
    
    subgraph "Vulkan Layer"
        REN[Renderer 底层]
        RM[ResourceManager]
    end
    
    APP --> WORLD
    RS --> REN
    REN --> RM
```

### Renderer 职责变化

ECS 后，`Renderer` 变成**底层 Vulkan 封装**，不包含场景逻辑：
- ❌ 不再遍历 Model/Primitives
- ❌ 不再持有 Camera
- ✅ 只负责 beginFrame/endFrame、绑定 Pipeline、录制 Draw

`RenderSystem` 成为新的渲染入口，负责查询 Entity、排序、调用底层 Renderer。

### Entity & World Management
- [ ] **Entity 管理器**
  - 实现 Entity ID 系统 (sparse set 或 generational index)
  - World/Registry 管理所有 entities 和 components
  - 考虑使用 [entt](https://github.com/skypjack/entt) 或自研

### Components (纯数据)
- [ ] **TransformComponent**
  - `glm::vec3 position, rotation, scale`
  - `glm::mat4 localMatrix, worldMatrix`
  - `EntityID parent` (层级关系)

- [ ] **MeshComponent**
  - `MeshHandle meshId` (指向 ResourceManager 中的 Mesh)
  - `AABB boundingBox`

- [ ] **MaterialComponent**
  - `MaterialHandle materialId`
  - 运行时材质参数覆盖

- [ ] **CameraComponent**
  - `float fov, nearPlane, farPlane`
  - `glm::mat4 viewMatrix, projMatrix`
  - `bool isActive`

- [ ] **LightComponent**
  - `LightType type` (Directional, Point, Spot)
  - `glm::vec3 color, float intensity`
  - `float range, innerCone, outerCone`

### Systems (逻辑)
- [ ] **TransformSystem**
  - 计算层级变换 (parent-child)
  - 更新所有 dirty 的 worldMatrix
  - 上传变换数据到 GPU (SSBO)

- [ ] **CameraSystem**
  - 更新活动相机的 view/projection 矩阵
  - 处理相机输入控制

- [ ] **LightSystem**
  - 收集场景中的所有灯光
  - 上传灯光数据到 GPU UBO/SSBO

- [ ] **CullingSystem**
  - 视锥体剔除 (Frustum Culling)
  - 遮挡剔除 (Occlusion Culling - 可选)
  - 输出可见 entity 列表

- [ ] **RenderSystem**
  - 从 CullingSystem 获取可见 entities
  - 按材质排序 (减少 state changes)
  - 录制绘制命令
  - 管理 render passes (Depth, Forward, UI)

- [ ] **UISystem**
  - ImGui 渲染
  - 处理 UI 输入

### Migration Path
1. 先实现 Components 和基础 World
2. 逐步将 Renderer 逻辑迁移到 RenderSystem
3. TransformSystem 替代当前的 Model 矩阵处理
4. 最后迁移 Camera 和 Light

## ✅ Archived

- [x] Integrate a simple logging system: use spdlog! (Completed 2025-12-27)
  - Added LogSystem class with spdlog integration
  - Multi-sink output: console (color), file, ringbuffer (for ImGui)
  - Source location tracking using std::source_location
  - Vulkan debug callback integration

