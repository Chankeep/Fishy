# 已修复的 Bug 记录

本文档记录项目中遇到并已修复的重要 Bug 及其根因分析。

## 1. 信号量重用错误 (Semaphore Reuse / Signal Semaphore)

### 错误现象
Validation Layer 报错：
`vkQueueSubmit(): pSignalSemaphores[0] ... is being signaled ... but it may still be in use by VkSwapchainKHR`

### 根因分析
这个错误源于 **CPU 逻辑帧（Frame）** 与 **交换链图像（Swapchain Image）** 之间生命周期的错位。

#### 核心概念：三个独立的角色
1.  **CPU/App**: 你的代码逻辑。
    *   资源管理：`MAX_FRAMES_IN_FLIGHT = 2` (Frame 0, Frame 1)。
    *   循环方式：`_currentFrameIndex = 0 -> 1 -> 0 -> 1 ...`
2.  **GPU**: 执行渲染命令。
3.  **显示器/呈现引擎 (Swapchain)**: 负责把画好的图显示在屏幕上。
    *   资源数量：`imageCount = 3` (Image A, Image B, Image C)。
    *   **关键点**：它显示的节奏可能很慢（比如垂直同步 60Hz），甚至被阻塞。

#### ❌ 错误做法：按帧绑定 (Per-Frame)
原代码中，`RenderFinishedSemaphore` 是 **FrameData** 的一部分，意味着一共只有 2 个信号量（Sem_F0, Sem_F1）。

**“车祸”现场模拟：**
1.  **Frame 0**: 使用 **Sem_F0** 提交渲染 (`vkQueueSubmit`)，然后交给呈现引擎显示 (`vkQueuePresent`)。
2.  **Frame 1**: 正常运行。
3.  **Frame 0 (再次轮到)**:
    *   CPU 等到了 Fence，说明 GPU 已经把上一帧 Frame 0 的命令画完了。
    *   但是！**呈现引擎可能还在持有 Sem_F0**（比如屏幕还没刷新完上一张 Frame 0 对应的图）。
    *   此时代码试图再次 `vkQueueSubmit` 并 **Signal** 这个 **Sem_F0**。
    *   **Crash**: Vulkan 禁止重用一个正在被 Wait 的信号量。

### 修复方案
将 `RenderFinishedSemaphore` 改为 **Per-Swapchain-Image**（跟图片走，绑定到 Swapchain Image 的数量，通常为 3 个）。

#### ✅ 正确做法：按图片绑定 (Per-Image)
逻辑变更：只有当我们通过 `AcquireNextImage` 拿到同一张图片时，才说明呈现引擎已经彻底用完了这张图，此时重用对应的信号量是安全的。

```cpp
// 1. 先问 Swapchain 要一张确实空闲的图
// 这一步是阻塞或保证安全的，Swapchain 绝对不会给你一张它还在用的图
uint32_t imageIndex = _swapChain->acquireNextImage(...);

// 2. 根据这张图的 ID (0, 1, 2) 来选择信号量
// 哪怕我是 Frame 0，如果图是 Image 2，我就用 Sem[2]
// 完全避开了还在忙碌的 Sem[0]（如果 Sem[0] 绑定的 Image A 还在显示的话）
submitInfo.pSignalSemaphores = &_renderFinishedSemaphores[imageIndex];
```

---

## 2. 描述符池无效错误 (Invalid Descriptor Pool)

### 错误现象
程序退出时报错：
`vkFreeDescriptorSets(): descriptorPool Invalid VkDescriptorPool Object`

### 根因分析
C++ 类成员变量的**析构顺序是声明顺序的逆序**。在使用 `vulkan_hpp` 的 RAII 包装器（`vk::raii`）时，这是一个常见的资源生命周期陷阱。
- 错误的代码顺序：`_frames`（包含 `DescriptorSet`）声明在 `_descriptorPool` 之前。
- 析构流程：
    1. `_descriptorPool` 先被析构，底层 Pool 对象被销毁。
    2. `_frames` 后被析构，触发 `DescriptorSet` 的析构函数。
    3. `DescriptorSet` 试图调用 `vkFreeDescriptorSets` 将自己归还给 Pool，但此时 Pool 已不存在，导致 Crash 或报错。

### 修复方案
调整 `Renderer` 类中成员变量的声明顺序。
- 将 `_frames` 移动到 `_descriptorPool` 之后声明。
- 确保析构顺序为：先析构 `_frames`（Sets 被安全归还），后析构 `_descriptorPool`。

## 3. 矩阵布局错位 (Matrix Layout Mismatch / Geometry Distortion)

### 错误现象
渲染出的模型严重变形，表现为被压缩成一个巨大的平面，或者坐标轴完全错乱。验证层无报错。

### 根因分析
**矩阵内存布局（Memory Layout）** 在 CPU 和 GPU 端的默认设置不一致：
1.  **CPU (GLM)**: 默认主要使用 **列主序 (Column-Major)**。
2.  **GPU (Slang/HLSL)**: 默认主要使用 **行主序 (Row-Major)**。

当直接将 GLM 的矩阵数据 memcpy 到 Uniform Buffer，并在 Shader 中以默认方式读取时，Shader 会把列主序数据当成行主序解析，导致**矩阵被转置** (Transposed)。
例如，投影矩阵（Projection Matrix）被转置后，Z 轴深度计算会完全失效。

### 修复方案
**方案 A (推荐 - 全局配置)**：
在 C++ 端初始化 Slang Session 时，强制指定默认布局为 Column-Major，以匹配 GLM。

```cpp
slang::SessionDesc sessionDesc = {};
// ...
sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR; // 关键配置
```

**方案 B (局部修饰)**：
在 Shader代码中显式为每个矩阵变量添加 `column_major` 修饰符（不推荐，繁琐且容易遗漏）。

```slang
// PBRshader.slang
struct GlobalUBO {
    column_major float4x4 view;
    column_major float4x4 proj; 
    // ...
};
```
