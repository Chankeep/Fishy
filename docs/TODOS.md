### 🚨 Phase 0: 紧急修复与止血 (Immediate Fixes)

*目标：修复内存安全隐患，消除显式同步等待，优化最明显的性能瓶颈。*




---

### 🏗️ Phase 1: 核心后端抽象 (Backend Abstraction)

*目标：将资源管理与渲染逻辑分离，建立“帧”的概念。*

* [ ] **提取 `FrameContext` (每帧资源)**
  * [ ] 创建 `core/FrameContext.h`。
  * [ ] 将 `commandBuffer`, `imageAvailableSemaphore`, `renderFinishedSemaphore`, `inFlightFence` 移入此结构。
  * [ ] 实现 `reset()` 方法：等待 Fence -> Reset Fence -> Reset CommandPool。
  * [ ] **新增：** 添加 `LinearAllocator` (std::vector<uint8_t> + offset) 用于每帧动态数据（Indirect Commands, Dynamic UBOs）。


* [ ] **创建 `RenderContext` (RHI 层)**
  * [ ] 创建 `core/RenderContext.h/cpp`。
  * [ ] 移入 `VulkanDevice`, `SwapChain`, `Surface` 的所有权。
  * [ ] 实现 `FrameContext& beginFrame()`：处理 `acquireNextImage` 和同步等待。
  * [ ] 实现 `void endFrame(FrameContext&)`：处理 `queueSubmit` 和 `queuePresent`。
  * [ ] 此时 `Renderer` 类应该不再持有 `_device` 和 `_swapChain` 的直接所有权，而是持有 `RenderContext` 的引用。


---

### 🚀 Phase 2: 数据导向管线 (DOD Data Pipeline)

*目标：将场景遍历逻辑移出渲染循环，利用 EnTT 系统。*

* [ ] **创建 `GeometrySystem` (ECS System)**
  * [ ] 在 `ecs/systems/` 下创建 `GeometrySystem`。
  * [ ] 移动 `buildInstanceDataFromScene` 和 `buildDrawBatchesFromScene` 的逻辑到 `GeometrySystem::update()`。
  * [ ] **优化：** 使用 `registry.group<MeshComponent, TransformComponent>(...)` 确保内存连续性。
  * [ ] **优化：** 仅在 Entity 变动或 Transform 变动时重建整个 Buffer（或实现 Dirty Flag）。


* [ ] **实现 GPU 数据上传逻辑**
  * [ ] `GeometrySystem` 输出 `std::span<InstanceData>` 和 `std::span<DrawCommand>`。
  * [ ] 利用 `FrameContext` 的 `LinearAllocator` 或 Staging Buffer 将数据上传到 GPU。
  * [ ] `Renderer` 现在只需要接收 `VulkanBuffer` 的句柄，而不需要知道 `Scene` 的存在。



---

### 🎨 Phase 3: 模块化渲染图 (Render Graph & Passes)

*目标：解耦“做什么”（Pass）和“怎么做”（Backend）。*

* [ ] **定义 `IRenderPass` 接口**
  * [ ] 定义 `RenderGraphData` 结构体（包含 CommandBuffer, Viewport, Global DescriptorSet）。
  * [ ] 定义接口：`execute(const RenderGraphData& data, entt::registry& registry)`。


* [ ] **拆分现有渲染逻辑**
  * [ ] **`MainDrawPass`**: 封装 `_graphicsPipeline`，负责主要的 Indirect Draw。
  * [ ] **`SkyboxPass`**: 封装 `_skyboxPipeline` 和 Skybox Mesh 渲染。
  * [ ] **`UIPass`**: 封装 ImGui 或 UI 回调逻辑。


* [ ] **重构 `Renderer` 为 `RenderGraphExecutor`**
  * [ ] `Renderer` 内部维护 `std::vector<std::unique_ptr<IRenderPass>>`。
  * [ ] `render()` 函数变为简单的循环：
```cpp
auto& frame = ctx.beginFrame();
// ... barriers ...
for(auto& pass : passes) pass->execute(...);
// ... barriers ...
ctx.endFrame(frame);

```





---

### 🔧 Phase 4: 高级特性与清理 (Polish & Advanced)

*目标：利用 Modern C++ 和 Vulkan 新特性进一步提升。*

* [ ] **Pipeline 管理**
  * [ ] 创建 `PipelineBuilder` 的缓存机制（Pipeline Library）。
  * [ ] 避免在 `Renderer` 构造函数中硬编码创建 Pipeline，改为在 Pass `init` 时按需创建。


* [ ] **引入 `vk::StructureChain**`
  * [ ] 检查所有 `pNext` 链的构建（如 `createBindlessTextureSetLayout`），改用 C++20 `vk::StructureChain` 以提高类型安全。


* [ ] **(Optional) Compute Shader Culling**
  * [ ] 在 `GeometrySystem` 和 `MainDrawPass` 之间插入一个 `CullPass`。
  * [ ] 使用 Compute Shader 生成 Indirect Draw Commands，而不是在 CPU 端生成。

## ✅ Archived

- [x] **Fix render loop heap allocations (REQ-P-06)** ✅ 2025-01-01
  - Replaced `std::vector<vk::DescriptorSet>` with stack-based binding
  - Moved Set 0 binding outside the loop (only Set 1 binds per-primitive)
  - Related: NFR-03 (Zero-allocation render loop)

- [x] **Implement Pipeline Cache (REQ-P-01, REQ-R-05)** ✅ 2025-01-01
  - Added `vk::raii::PipelineCache` to `Renderer`
  - Load cache from disk on startup (`pipeline_cache.bin`)
  - Save cache to disk on shutdown
  - Pass cache to `PipelineBuilder::build()`

- [x] **Instanced/Indirect Rendering (REQ-P-04)** ✅ 2025-01-01
  - Group primitives by material for instanced rendering
  - Implement `vkCmdDrawIndexedIndirect`
  - Reduce draw call overhead for 500+ objects (NFR-01)

- [x] **Fix PBR material texture formats** ✅ 2025-01-01
  - Fixed `metallicRoughnessTexture` to use `eR8G8B8A8Unorm` (data texture, not color)
  - Fixed `occlusionTexture` to use UNORM (data texture)
  - Fixed `clearcoatTexture/clearcoatRoughnessTexture` to use UNORM
  - Fixed `transmissionTexture` to use UNORM

- [x] **Fix shader alphaRoughness calculation** ✅ 2025-01-01
  - Changed `float alphaRoughness = perceptualRoughness * perceptualRoughness;`
  - Correct PBR specular highlights

- [x] **Optimize Descriptor Set Binding** ✅ 2025-01-01
  - Bind Set 0 once outside the loop, only bind Set 1 (Material) per primitive
  - Reduces redundant descriptor set binding overhead

- [x] Integrate a simple logging system: use spdlog! ✅ 2025-12-27
  - Added LogSystem class with spdlog integration
  - Multi-sink output: console (color), file, ringbuffer (for ImGui)
  - Source location tracking using std::source_location
  - Vulkan debug callback integration

- [x] **Bindless Descriptors (REQ-P-05)** ✅ 2025-01-03
  - Implement Descriptor Indexing extension
  - Use texture arrays with material indices
  - Reduce descriptor set switching overhead

- [x] **Vertex/Index Buffer Batching** ✅ 2025-01-03
  - Merge vertex/index data into single large buffers per scene
  - Use `firstIndex` and `vertexOffset` parameters instead of rebinding
  - Implemented via `Renderer::buildUnifiedBuffers`

- [x] **Per-Object Model Matrix with Bindless SSBO** ✅ 2025-01-03
  - Implemented `g_Instances` SSBO in Set 2
  - Supports separate transform for every object instance

- [x] **Material Sorting / Batching** ✅ 2025-01-03
  - Superceded by Bindless implementation
  - Unified draw calls reduce need for frequent descriptor switches

- [x] **Proper Inverse Normal Matrix for Non-Uniform Scaling** ✅ 2025-01-03
  - Fixed in PBR shader using normal matrix from SSBO instance data

- [x] **Image-Based Lighting (IBL)** ✅ 2025-01-03
  - Add environment cubemap support
  - Pre-filtered environment map for specular
  - Irradiance map for diffuse
  - Integrates with existing PBR shader

- [x] **封装 VMA Image (RAII Wrapper)** ✅ 2025-01-10
  - [x] 创建 `core/VulkanImage.h` 类。
  - [x] 将 `vmaCreateImage` 和 `vmaDestroyImage` 封装在构造/析构函数中。
  - [x] 确保 `vk::raii::ImageView` 的成员变量声明顺序在 `VulkanImage` 之后（保证析构顺序：View -> Image）。
  - [x] **关键修复：** 移除 `Renderer::~Renderer` 中手动调用 `vmaDestroyImage` 的代码。


- [x] **优化 Bindless 纹理索引查找** ✅ 2025-01-10
  - [x] 修改 `Material` 结构体，增加 `int32_t baseColorIndex`, `int32_t normalIndex` 等字段。
  - [x] 在 `ResourceManager::loadMaterial` 或材质创建时，一次性计算好这些 Index。
  - [x] **性能提升：** 在 `Renderer::buildInstanceDataFromScene` 中，移除所有 `getTextureBindlessIndex` 的哈希查找，直接读取整数。


- [x] **移除 SwapChain 重建时的 `DeviceWaitIdle`** ✅ 2025-01-10
  - [x] 修改 `recreateSwapChain`，仅等待旧 SwapChain 相关的 Fences。
  - [x] 确保 `_frames` 资源在重建期间被正确回收或保留。