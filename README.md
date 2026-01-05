# 🐟 Fishy Engine

一个基于现代 Vulkan (C++20) 和 ECS 架构的轻量级渲染引擎。

## 🎯 设计哲学

**"Simple & Good"**

我们追求代码的清晰与显式，拒绝隐式的"Magic"。
*   **架构清晰**: 严格遵循 ECS 架构 (EnTT)，数据 (Component) 与逻辑 (System) 分离。
*   **显式编排**: `Application::run` 显式控制 Frame Loop，逻辑流向一目了然。
*   **现代堆栈**: 全面拥抱 C++20 与 Vulkan 1.3+ 生态。

## ✨ 技术栈

*   **Language**: C++20
*   **Architecture**: ECS (Entity-Component-System) via [EnTT](https://github.com/skypjack/entt)
*   **Graphics**: Vulkan 1.3+ (Dynamic Rendering)
    *   **Meta-loader**: `volk`
    *   **Bootstrap**: `vk-bootstrap`
    *   **API Wrapper**: `vulkan-hpp` (RAII)
*   **Assets**:
    *   **Models**: `tinygltf` (glTF 2.0)
    *   **Textures**: `stb_image` / `ktx`
*   **Math**: `glm`
*   **UI**: `Dear ImGui` (Docking)
*   **Logging**: `spdlog`

## 🚀 已实现功能

### 核心架构
- ✅ **ECS 系统集成** (EnTT Registry, System/Component 分离)
- ✅ **多帧并行渲染** (Frames in Flight)
- ✅ **RAII 资源管理** (自动处理 Vulkan 对象生命周期)
- ✅ **统一日志系统** (Console + File + Ringbuffer Sink)

### 渲染特性
- ✅ **PBR 渲染管线** (Cook-Torrance BRDF)
- ✅ **IBL (Image Based Lighting)** (环境光照)
- ✅ **glTF 2.0 模型加载** (Mesh, Material, Texture)
- ✅ **ImGui 编辑器交互** (实时调整参数)

### 资源系统
- ✅ **Vulkan Buffer/Image 封装**
- ✅ **纹理自动加载** (支持嵌入式及外部文件)

## 📂 项目结构

```
Fishy/
├── src/
│   ├── core/              # 核心模块 (App, Window, VulkanContext)
│   ├── ecs/               # ECS 架构
│   │   ├── components/    # 纯数据组件 (Transform, Mesh, Camera...)
│   │   └── systems/       # 无状态系统 (Render, Transform, Lighting...)
│   ├── renderer/          # 渲染后端 (Vulkan Wrapper, Pipeline)
│   ├── resources/         # 资源加载 (ModelLoader, IBL...)
│   ├── scene/             # 场景管理 (Scene 包装类)
│   └── ui/                # ImGui 层
├── assets/                # 资源文件 (Models, Shaders, Environments)
└── docs/                  # 开发文档
```

## 🛠️ 构建指南

### 前置要求
- **Vulkan SDK 1.3+**
- **C++20 编译器** (MSVC 2022 v17+ / GCC 11+ / Clang 13+)
- **CMake 3.20+**

### 步骤
```bash
# 1. 克隆仓库
git clone https://github.com/your-repo/Fishy.git
cd Fishy

# 2. 生成工程
mkdir build && cd build
cmake ..

# 3. 编译
cmake --build . --config Release
```


## 📄 License

MIT
