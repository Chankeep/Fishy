# 🐟 Fishy Engine

一个基于现代 Vulkan 的轻量级渲染引擎。

## 🎯 项目目标

构建一个现代 Vulkan 渲染器原型，验证以下核心架构：

- **多帧并行渲染** - Frames in Flight 架构，最大化 GPU 利用率
- **模块化资源管理** - 统一的 Buffer、纹理、描述符管理
- **基础 PBR 渲染流程** - 物理基础渲染管线
- **GPU-Driven 预留** - 为后续 Mesh Shader / GPU Culling 打基础

## ✨ 特性

### 核心技术

- **Vulkan 1.4** - 现代图形 API，支持 Dynamic Rendering
- **C++20** - 利用最新语言特性 (std::source_location, concepts, modules)
- **vulkan-hpp RAII** - 自动资源管理，代码更安全
- **vk-bootstrap** - 简化的 Vulkan 初始化与设备选择
- **volk** - Meta loader，动态加载函数指针，提升扩展兼容性与调用性能
- **GLFW** - 跨平台窗口管理
- **GLM** - 数学计算库
- **spdlog** - 高性能日志系统
- **tinygltf** - glTF 2.0 模型加载
- **stb_image** - 图片加载

### 已完成功能

**核心系统:**
- ✅ 窗口化 (GLFW 集成)
- ✅ Vulkan 初始化 (vk-bootstrap + volk)
- ✅ 交换链管理 (创建/重建/窗口 Resize)
- ✅ 多帧并行架构 (Frames in Flight)
- ✅ 日志系统 (spdlog + 多 Sink 输出)
  - 控制台彩色输出
  - 文件持久化
  - 循环缓冲区 (ImGui 集成)
  - Vulkan Debug Callback 集成

**渲染系统:**
- ✅ 前向渲染 Pass (深度测试/写入)
- ✅ 动态状态 (Viewport/Scissor)
- ✅ Shader 管理 (SPIR-V 加载)
- ✅ 图形管线 (Graphics Pipeline)

**资源管理:**
- ✅ Buffer 封装 (VulkanBuffer + Staging 上传)
- ✅ 纹理系统 (stb_image + 多格式支持)
  - 外部文件加载 (.jpg, .png)
  - 嵌入式 buffer (glTF 内嵌)
  - 内存数据加载
- ✅ glTF 模型加载 (tinygltf)
  - 节点层级遍历
  - 动态属性收集 (POSITION, NORMAL, TEXCOORD_0, TANGENT, 等)
  - 嵌入式数据支持
- ✅ PBR 材质系统
  - Metallic-Roughness 工作流
  - 纹理: baseColor, normal, metallicRoughness, occlusion, emissive
  - KHR 扩展支持: clearcoat, transmission, ior, emissive_strength
- ✅ ResourceManager (GPU 资源缓存)

### 进行中

- 🔄 ImGui 集成 (编辑器 UI)
- 🔄 PBR Shader 实现
- 🔄 描述符管理优化
- 🔄 KTX2 纹理支持

### 计划中

- ⏳ 相机漫游系统
- ⏳ 场景管理
- ⏳ 材质编辑器
- ⏳ 资源浏览器

## 🛠️ 构建

### 前置要求

- Vulkan SDK 1.3+
- C++20 编译器 (MSVC 2022+推荐, GCC 11+, Clang 13+)
- CMake 3.20+
- Ninja 或 Make

### 构建步骤

```bash
# 克隆仓库
git clone <repo_url> Fishy
cd Fishy

# 配置与构建
mkdir build && cd build
cmake .. -G Ninja
ninja

# 或使用 Make
cmake ..
make -j$(nproc)
```

## 🚀 运行

```bash
cd build/bin
./Fishy
```

## 📁 项目结构

```
Fishy/
├── src/
│   ├── core/              # 核心模块
│   │   ├── Application    # 应用程序入口
│   │   ├── VulkanContext  # Vulkan 上下文管理
│   │   ├── VulkanDevice   # 设备管理
│   │   ├── VulkanBuffer   # Buffer 封装
│   │   ├── Window         # 窗口管理
│   │   ├── SwapChain      # 交换链管理
│   │   └── LogSystem      # 日志系统
│   ├── renderer/          # 渲染器
│   │   ├── Renderer       # 渲染器主类
│   │   ├── GraphicsPipeline
│   │   ├── PipelineBuilder
│   │   ├── Material       # PBR 材质
│   │   └── CommandPool
│   ├── resources/         # 资源管理
│   │   ├── ResourceManager
│   │   ├── ModelLoader    # glTF 模型加载
│   │   ├── Model / Mesh
│   │   └── Texture
│   └── ui/                # 用户界面
│       ├── ImGuiLayer
│       └── DebugPanel
├── shaders/               # Shader 源码
├── docs/                  # 文档
│   ├── 需求规格说明书 (PRD).md
│   ├── 系统设计说明书.md
│   └── bugs.md
├── TODOS.md               # 任务列表
└── CMakeLists.txt         # CMake 配置
```

## 📋 依赖

### 核心依赖
- **Vulkan SDK** (1.3+) - 图形 API
- **GLFW** (3.3+) - 窗口/输入管理
- **GLM** - 数学库

### FetchContent 自动获取
- **vk-bootstrap** - Vulkan 初始化简化
- **volk** - Vulkan 函数加载器
- **spdlog** - 日志系统
- **tinygltf** - glTF 模型加载
- **stb_image** - 图片加载

## 📊 架构亮点

### 动态资源加载
- 顶点属性自动发现和收集
- 纹理加载方式自动识别 (外部文件/嵌入式 buffer/已解码内存)
- 扩展友好的材质系统

### 统一日志系统
```cpp
// 自动记录源位置 (文件名、行号、函数名)
LogSystem::get().info("Loaded model: {} with {} primitives", filepath, count);
LogSystem::get().trace("Mesh attributes: [{}]", attrsStr);
LogSystem::get().error("Failed to load texture: {}", path);
```

## 📄 文档

- [需求规格说明书 (PRD)](docs/需求规格说明书%20(PRD).md)
- [系统设计说明书](docs/系统设计说明书.md)
- [Bug 追踪](docs/bugs.md)

## 📄 License

MIT
